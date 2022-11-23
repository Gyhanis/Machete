# Machete 
Machete: Lossy compressor designed for Time Series Databases.

## Contents

* bin/chimptest.jar: Executable jar of 
[Chimp128](https://github.com/panagiotisl/chimp.git) testing 
program.
* client/: Source codes of InfluxDB client programs that test 
the performance of InfluxDB.
* data/: Sample data 
* gorilla/: Source Code of Gorilla compressor testing program.
gorilla.go file is copied from 
[InfluxDB](https://github.com/influxdata/influxdb.git).
* influxdb/: Variants of InfluxDB
* lib/libSZ.so: [SZ compressor](https://github.com/szcompressor/SZ.git) 
Library.
* machete/: Source code of Machete.
* other/config.properties: config file of 
[IoTDB-Benchmark](https://github.com/thulab/iotdb-benchmark.git) used in our paper.
* other/*server.c: SZ compressor deamon source code.
* other/test_chimp.java: Source code of bin/chimptest.jar
* other/test_sz: Source code of SZ testing program.
* scripts/: Scripts for build and running test.
* SZ/: Headers of SZ compressor.
* .gitignore
* .gitmodules
* influxdb-cfg.yml: Extra configuration file of InfluxDB variants.
* README.md

## Requirements

### Building Machete

* __CPU__: Supporting SIMD Instruction sets including: 
SSE, SSE2, AVX, AVX2, AVX-512F, AVX-512DQ, AVX-512VL
* __GCC__: Not sure about the minimum required version, works with 12.2.0.
* __make__

### Building InfluxDB Variants

See [Contributing to InfluxDB v2](https://github.com/influxdata/influxdb/blob/master/CONTRIBUTING.md#building-from-source)

## Running Standalone Tests

Standalone tests runs tests of Gorilla, Chimp128, SZ, and Machete 
outside a database.

Notice: Do NOT `cd` into the "scripts" directory when running scripts in it, 
run it in current directory. The scripts are straightforward, so it should be
easy to learn what they does by reading them and make adjustments.

1. Run `scripts/build_standalone`. After this step, following files should be 
generated:
    - bin/gorilla
    - bin/mach 
    - bin/sztest
    - lib/libmachete.a
    - lib/libmachete.so
2.  Run `scripts/test_standalone`. 

## Running Tests with InfluxDB Variants

1. Run `scripts/build_database`. After this step, following files 
should be generated:
    - bin/deco_server: SZ decoding daemon.
    - bin/enco_server: SZ encoding daemon.
    - bin/influxd-xx(xx could be ori, sz, void, or mach)
    - bin/influxd-xx-nc
    - bin/szserver: SZ compression daemon.
2. Run `scripts/run_szserver <num>` in another terminal to start the SZ daemon. "num"
is the process count to be created for compression/decompression service. 8 is used in 
our paper. 
3. Run `scripts/run_influxd <ver>` to start a InfluxDB server. "ver"
cound be one of the following:
    - _ori_: Original InfluxDB that uses Gorilla.
    - _sz_: InfluxDB that uses SZ compressor.
    - _mach_: InfluxDB that uses Machete.
4. Create the first account in InfluxDB of organization name "test".
Create a bucket named "test". 
5. Fill the access token of InfluxDB in client/query/main.go and 
client/write/main.go. Then run `scripts/build_client`. Following files 
should be generated:
    - client/query/query
    - client/write/write
6. Run `client/write/write <client_cnt> <data_dir> <output_dir>` to run writing tests.
"output_dir" is the directory where output files should be placed, which should be created
beforehand. 
Run `client/query/query <client_cnt> <query_file> <output_dir>` to run query tests.
Example:
```
mkdir result
client/write/write 4 data/write/System result
client/query/query 8 data/query/System result
```

### Notice
* influxd-xx-nc are modified to return only the amount of relevant records 
and time spend on query. client/write/write and client/query/query assumes 
that current database is influxd-xx-nc.
* influxd-xx should be used when using IoTDB-Benchmark.
* SZ Daemon is only required by influxd-sz(-nc).
* influxd-xx(-nc) will search for influxdb-cfg.yml at current directory.
Entries in influxdb-cfg.yml includes:
    - ThreadCount: Maximum kernel threads allowed.
    - ErrorBound: Error bound of lossy compressor. Ingored if not using lossy compressor.
    - BLFile: output file used to place the compressed block sizes
    (blocks with 1000 data only).
    - TimingFile: output file of some timing measurement.  

## Others
For considerations like license, only part of data is published in this repository.
