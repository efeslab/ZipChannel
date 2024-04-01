This figure is not incuded in the published version of the paper, however it provides a sinmple example to test TaintChannel on.

Files in this folder explained:
## [code.c](code.c), [file.txt](file.txt), [Makefile](Makefile)
A simple application to read [file.txt](file.txt) into a buffer, and build an histogram of all of the bytes in the file. This code is losely based on the vulnerability in Bzip2 (but extremely simplified). Run Makefile to compile this file.

## [gen_log.sh](gen_log.sh)
Run DynamoChannel to detect the vulnerabilities in the previously generated binaries. Assumes you already  compiled DynamoRIO and TaintChannel.

## [log2svg.sh](log2svg.sh)
Generates an SVG file from the output of `gen_log.sh`. Not neccesary for normal usage, but we used this to generate figures in the paper.
