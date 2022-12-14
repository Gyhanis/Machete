package main

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"path"
	"strconv"

	db "github.com/influxdata/influxdb-client-go/v2"
)

const token string = "access token here"

const max_thread_cnt = 32
const query_cnt = 100000

type report struct {
	time  float64
	count int64
}

var thread_report chan (report)
var query_report chan (report)

var query_list chan (string)

// var valid_query chan (string)

func query_thread() {
	client := db.NewClient("http://localhost:8086", token)
	defer client.Close()
	querier := client.QueryAPI("test")
	rep := report{0, 0}
	for {
		query, ok := <-query_list
		if !ok {
			thread_report <- rep
			return
		}
		result, err := querier.QueryRaw(context.Background(), query, db.DefaultDialect())
		if err != nil {
			thread_report <- report{-1, -1}
			return
		}
		var qrep report
		fmt.Sscanf(result, "%v\n%v\n", &qrep.time, &qrep.count)
		if qrep.count == 0 {
			continue
		}
		rep.time += qrep.time
		rep.count += qrep.count

		query_report <- qrep
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
// os.Args[2] query data file
// os.Args[3] output dir

func main() {
	thread_count, err := strconv.Atoi(os.Args[1])
	check_err(err)
	if thread_count > max_thread_cnt {
		fmt.Println("Thread count exceeds\n")
		os.Exit(1)
	}

	query_file, err := os.Open(os.Args[2])
	check_err(err)

	scan := bufio.NewScanner(query_file)
	query_list = make(chan string, query_cnt+1)
	for scan.Scan() {
		query_list <- scan.Text()
	}
	query_file.Close()
	close(query_list)

	fqlat, err := os.Create(path.Join(os.Args[3], "query_latency"))
	check_err(err)
	fqcnt, err := os.Create(path.Join(os.Args[3], "query_count"))
	check_err(err)

	thread_report = make(chan report, thread_count)
	defer close(thread_report)
	query_report = make(chan report, thread_count)
	defer close(query_report)

	for i := 0; i < thread_count; i++ {
		go query_thread()
	}

	working_threads := thread_count
	time_consumed := float64(0)
	query_processed := 0
query_listening:
	for {
		select {
		case r := <-thread_report:
			if r.time < 0 {
				fmt.Println("Error occured while querying!")
			} else {
				if r.time > time_consumed {
					time_consumed = r.time
				}
			}
			working_threads--
		case r := <-query_report:
			fmt.Fprintf(fqlat, "%v\n", r.time)
			fmt.Fprintf(fqcnt, "%v\n", r.count)
			query_processed++
			fmt.Fprintf(os.Stderr, "Query processed: %v\r", query_processed)
		default:
			if working_threads == 0 {
				fqlat.Close()
				fqcnt.Close()
				write_file(time_consumed, path.Join(os.Args[3], "query_time"))
				break query_listening
			}
		}
	}
	fmt.Printf("Query finished in %v seconds\n", float64(time_consumed)/1000/1000)
	fmt.Printf("Job done\n")
}
