# Machete
Machete: Lossy compressor designed for Time Series Databases.

## Features
* Point-wise absolute error control
* Improved compression ratio for small error bounds
* Improved compression ratio for short input data
* High decompression speed

## Running Evaluation

### Requirements

* GNU Make 
* GNU g++ compiler

### Quick Start

* Use command ```make ``` to build executable compression_test
* Use command ```./compression_test``` to run the evaluation

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

* Machete: a novel lossy compressor 
* Gorilla: a fast lossless time series database compressor
* Chimp~1~: 
