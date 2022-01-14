# iotest

__Motivation for Building this Library__

* One tool for different use cases.
* Different test tools should provide same output format.
* Creating own tests for better understanding how IO works.
* Other tools have restricted support for using greater random data blocks...
  * IOZone is restricted to 16M record size.
  * Creating random data with DD can take huge amount of time or workarounds need to be done.
* Direct IO is not always possible e.g. on ZFS, so big record size with random data should be used when compression is activated.
* Random generated bits used for tests should be hardly compressible, it's creations should be fast and the data useable again.

__Output Format__

Fields:  
* start_timestamp (format: "YYYY-MM-DD HH:MM:SS")
* end_timestamp (format: "YYYY-MM-DD HH:MM:SS")
* elapsed_time (seconds)
* throughput (MB/s) 
* description (depending on the test some meta infromation is provided)

The output fields are pipe separated e.g.:  

    2020-10-26 11:40:17|2020-10-26 11:40:17|0|10|seq_io_test-write-1M-10M

## Sequential IO Test (seq\_io\_test)

Write or read a total size with a given block size to or from a file.  
no count must be specified, since the count is calculated from the total and block size  
(block size must be multiple of the total size).

### Build

Make static build so it can be used on other machines without compiling again, but with same architecture:  

`g++ src/seq_io_test.cpp -o bin/seq_io_test -ansi -pedantic -Wall -Wextra -std=c++11 --static`

### Usage

`./bin/seq_io_test -h`  

    USAGE:
    -b BLOCK SIZE 1-999{KM}/1G
    -t TOTAL SIZE 1-999{KMGT}
    -r/w READ/WRITE MODE
    -f FILEPATH
    -S SEED NUMBER (optional)

