# iotest

## Build

Make static build so it can be used on other machines without compiling:  

`g++ src/seq_io_test.cpp -o bin/seq_io_test -ansi -pedantic -Wall -Wextra -std=c++11 --static`