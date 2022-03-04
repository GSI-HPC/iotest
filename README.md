# iotest

## Motivation

* One tool for different use cases.
* Different test tools should provide same output format.
* Creating own tests for better understanding how IO works.
* Other tools have restricted support for using greater random data blocks...
  * IOZone is restricted to 16M record size.
  * Creating random data with DD can take huge amount of time or workarounds need to be done.
* Direct IO is not always possible e.g. on ZFS, so big record size with random data should be used when compression is activated.
* Random generated data used for tests should be hardly compressible, it's creations should be fast and the data useable again.

## Build

Make static build so it can be used on other machines without compiling again, but with same architecture:  

`g++ src/iotest.cpp -o bin/iotest -ansi -pedantic -Wall -Wextra -std=c++11 --static`

## Usage

`./bin/iotest -h`  

```
USAGE:
-b BLOCK SIZE 1-999{KM}/1G
-t TOTAL SIZE 1-999{KMGT}
IO MODES:
   * -r SEQUENTIAL READ
   * -w SEQUENTIAL WRITE
   * -R RANDOM READ
   * -W RANDOM WRITE
-f FILEPATH
-S SEED
-Y SYNC ON WRITE
```

For random write mode other seed should be used rather than used for sequential write with same block size,  
otherwise same block data will be written, which might be optimized on writes.

## Output Format

Fields:  
* start\_timestamp (format: "YYYY-MM-DD HH:MM:SS")
* end\_timestamp (format: "YYYY-MM-DD HH:MM:SS")
* elapsed\_time (seconds)
* throughput (MB/s) 
* description (depending on the test some meta infromation is provided)

The output fields are pipe separated:  

```
2022-03-04 12:03:56|2022-03-04 12:05:09|73|14|sequential-sync-write-1M-1G|/media/hdd/test.tmp|
```
