# Machete
Machete: Lossy compressor designed for Time Series Databases. (Accepted by Data Compression Conference 2024)

## Features
* Point-wise absolute error control
* Improved compression ratio for small error bounds
* Improved compression ratio for short input data
* High decompression speed

## Running Evaluation

### Requirements

* GNU Make
* GNU g++ compiler
* ZSTD library, Zlib library (likely to be already installed on Linux, not required when building Machete library only)

### Quick Start

* Use command ```make ``` to build executable compression_test
* Use command ```./compression_test``` to run the evaluation
* To build only the Machete library (libmach.a), use ```make lib/libmach.a```, and *libmach.a* can be found in *lib* directory if compilation successes.

### Dataset

Only the System dataset is provided in this repo because we are not sure whether we have the right to distribute the others.

Other datasets can be found in:
* GeoLife: https://www.microsoft.com/en-us/download/details.aspx?id=52367
* ~~REDD: http://redd.csail.mit.edu~~ (Seems to be unavailable now, related article: *REDD: A Public Data Set for Energy Disaggregation Research*)
* Stock: https://zenodo.org/record/3886895\#.Y3H3QnZBybj

The evaluation loads data file under the given diretory, please ensure the directory contains *only* data file.
Each data file should be storing one series of data as binary double array.
In other words, the data file should be written in a way like:

        double data[LEN];
        fwrite(data, sizeof(data[0]), LEN, f_out);


### Evaluated Compressors

* Gorilla: a fast lossless time series database compressor. Codes in the repo are written based on the implementation in InfluxDB: https://github.com/influxdata/influxdb.git
* Chimp128: a lossless compressor based on Gorilla. Codes in this repo are based on https://github.com/panagiotisl/chimp.git
* Elf: a lossless compressor based on Gorilla improved for digital-place-limited data. Codes in this repo are based on https://github.com/Spatio-Temporal-Lab/elf
* ZStandard: version 1.3.3 
* Deflate(A.K.A. GZip): version 1.2.11
* SZ3: Code from https://github.com/szcompressor/SZ3.git
* LFZip: Code based on https://github.com/shubhamchandak94/LFZip.git
* Machete: a novel lossy compressor proposed by us.

### Evaluation Setting

The evaluation setting is in the 47th to 103rd lines in compression_test.cpp.
