package main

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"os/exec"
	"path"
	"strconv"
	"strings"
	"time"

	db "github.com/influxdata/influxdb-client-go/v2"
	db_w "github.com/influxdata/influxdb-client-go/v2/api/write"
)

const max_thread_cnt = 32
const batch_size = 1000

type report struct {
	total_time int64
}

var file_list chan (string)
var thread_report chan (report)
var latency_report chan (int64)

func recordToPoint(r string) *db_w.Point {
	// fmt.Printf("Parsing %v\n", r)
	sections := strings.Split(r, " ")
	parts := strings.Split(sections[0], ",")
	measurement := parts[0]
	parts = parts[1:]
	tags := make(map[string]string, len(parts))
	for _, t := range parts {
		tmp := strings.Split(t, "=")
		tags[tmp[0]] = tmp[1]
	}
	parts = strings.Split(sections[1], ",")
	fields := make(map[string]interface{}, len(parts))
	for _, f := range parts {
		tmp := strings.Split(f, "=")
		data, err := strconv.ParseFloat(tmp[1], 64)
		check_err(err)
		fields[tmp[0]] = data
	}
	_ts, err := strconv.ParseInt(sections[2], 10, 64)
	check_err(err)
	ts := time.Unix(_ts, 0)
	return db.NewPoint(measurement, tags, fields, ts)
}

func write_thread() {
	client := db.NewClient("http://localhost:8086", "LpIK5qpW7hnf0TjznaWRlALUxSwqM54hxvVBvukABe5w1R-_DovO9ws9UMt5wroQY8lGOK1SBK7kfUlquO0bZA==")
	defer client.Close()
	writer := client.WriteAPIBlocking("test", "test")

	var rep report
	// data := make([]string, 0, batch_size)
	points := make([]*db_w.Point, 0, batch_size)
	for {
		// fmt.Println("Acquiring file")
		fileName, ok := <-file_list
		if !ok {
			thread_report <- rep
			return
		}
		// fmt.Println("Writing file: ", fileName)
		file, err := os.Open(fileName)
		if err != nil {
			thread_report <- report{-1}
			return
		}
		defer file.Close()
		scanner := bufio.NewScanner(file)
		for {
			points = points[:0]
			for i := 0; i < batch_size; i++ {
				if scanner.Scan() {
					points = append(points, recordToPoint(scanner.Text()))
				} else {
					break
				}
			}
			if len(points) == 0 {
				break
			}
			// fmt.Printf("writing batch of size %v\n", len(points))
			start := time.Now()
			// err := writer.WriteRecord(context.Background(), line)
			err := writer.WritePoint(context.Background(), points...)
			dur := time.Since(start)
			if err != nil {
				fmt.Println(err)
			}
			latency_report <- dur.Microseconds()
			rep.total_time += dur.Microseconds()
		}
	}
}

func write_file(stuff interface{}, fname string) {
	file, err := os.Create(fname)
	check_err(err)
	fmt.Fprintf(file, "%v\n", stuff)
	file.Close()
}

func check_err(e error) {
	if e != nil {
		fmt.Println(e.Error())
		os.Exit(1)
	}
}

// os.Args[1] thread_cnt
// os.Args[2] write data dir
// os.Args[3] query data file
// os.Args[4] output dir

func main() {
	thread_count, err := strconv.Atoi(os.Args[1])
	check_err(err)
	if thread_count > max_thread_cnt {
		fmt.Println("Thread count exceeds\n")
		os.Exit(1)
	}
	datadir, err := os.Open(os.Args[2])
	check_err(err)
	dents, err := datadir.ReadDir(-1)
	check_err(err)
	datadir.Close()

	// fmt.Println("Loading file names")
	file_list = make(chan string, len(dents))
	for _, dentry := range dents {
		file_list <- path.Join(os.Args[2], dentry.Name())
	}
	close(file_list)
	// fmt.Println("File name loaded")

	thread_report = make(chan report, thread_count)
	defer close(thread_report)
	latency_report = make(chan int64, thread_count)
	defer close(latency_report)
	fwlat, err := os.Create(path.Join(os.Args[4], "write_latency"))
	check_err(err)

	time_consumed := int64(0)
	for i := 0; i < thread_count; i++ {
		go write_thread()
	}

	working_threads := thread_count
write_listening:
	for {
		select {
		case r := <-thread_report:
			if r.total_time < 0 {
				fmt.Println("Error occured while writing!")
			} else {
				if r.total_time > time_consumed {
					time_consumed = r.total_time
				}
			}
			working_threads--
		case latency := <-latency_report:
			// fmt.Println(".")
			fmt.Fprintf(fwlat, "%v\n", latency)
		default:
			if working_threads == 0 {
				fwlat.Close()
				write_file(time_consumed, path.Join(os.Args[4], "write_time"))
				break write_listening
			}
		}
	}
	fmt.Printf("Write finished in %v seconds\n", float64(time_consumed)/1000/1000)

	// fmt.Println("Calling backup")
	cmd := exec.Command("influx", "backup", "--bucket", "test", "backup")
	cmd.Dir = "/home/sy/tstest3"
	out_file, err := os.Create("backup_log")
	check_err(err)
	cmd.Stderr = out_file
	cmd.Stdout = out_file
	start := time.Now()
	err = cmd.Run()
	dur := time.Since(start)
	check_err(err)
	out_file.Close()
	write_file(dur.Microseconds(), path.Join(os.Args[4], "backup_time"))
	fmt.Printf("Backup finished in %v seconds\n", dur.Seconds())
}
