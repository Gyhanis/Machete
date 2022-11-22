package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"reflect"
	"strconv"
	"time"
	"unsafe"
)

const MAX_SIZE = 1000000

var data [MAX_SIZE]float64
var data2 [MAX_SIZE]float64

var comptime float64 = 0
var decomptime float64 = 0
var ori_size int = 0
var comp_size int = 0

func main() {
	files, _ := ioutil.ReadDir(os.Args[1])
	slice_size, _ := strconv.Atoi(os.Args[3])
	for _, file := range files {
		if file.IsDir() {
			continue
		}
		// fmt.Println(file.Name())
		blocks := int(file.Size()-1) / slice_size / 8
		rest := (int(file.Size()) - blocks*slice_size*8) / 8
		var buf []byte
		buf_head := (*reflect.SliceHeader)(unsafe.Pointer(&buf))
		buf_head.Cap = slice_size * 8
		buf_head.Len = slice_size * 8
		buf_head.Data = (uintptr)(unsafe.Pointer(&data[0]))

		fin, _ := os.Open(os.Args[1] + "/" + file.Name())
		data_slice := data[:slice_size]
		data_slice2 := data2[:slice_size]
		for i := 0; i < blocks; i += 1 {
			// fmt.Printf("ori_size: %v\n", ori_size)
			ori_size += slice_size
			fin.Read(buf)
			comp := make([]byte, 0)
			start := time.Now()
			comp, _ = FloatArrayEncodeAll(data_slice, comp)
			dur := time.Since(start)
			comp_size += len(comp)
			comptime += dur.Seconds()
			start = time.Now()
			FloatArrayDecodeAll(comp, data_slice2)
			dur = time.Since(start)
			decomptime += dur.Seconds()
			check(slice_size)
		}
		data_slice = data[:rest]
		data_slice2 = data2[:rest]
		if rest > 20 {
			ori_size += rest
			fin.Read(buf)
			comp := make([]byte, 0)
			start := time.Now()
			// comp := make([]byte, 0)
			comp, _ = FloatArrayEncodeAll(data_slice, comp)
			dur := time.Since(start)
			comptime += dur.Seconds()
			comp_size += rest
			start = time.Now()
			FloatArrayDecodeAll(comp, data_slice2)
			dur = time.Since(start)
			decomptime += dur.Seconds()
			check(rest)
		}
		fin.Close()
	}
	fmt.Printf("Test finished\n")
	fmt.Printf("Gorilla compression ratio: %v\n", float64(ori_size*8)/float64(comp_size))
	fmt.Printf("Gorilla compression speed: %v\n", float64(ori_size*8)/comptime/1024/1024)
	fmt.Printf("Gorilla decompression speed: %v\n", float64(ori_size*8)/decomptime/1024/1024)
}

func check(len int) {
	for i := 0; i < len; i++ {
		if data[i] != data2[i] {
			fmt.Printf("Error %v : %v vs %v\n", i, data[i], data2[i])
			os.Exit(-1)
		}
	}
}
