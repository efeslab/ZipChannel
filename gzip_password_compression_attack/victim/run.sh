#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: ./run.sh <filename to compress>"
  echo "       For example, ./run killer.txt"
    exit 1
fi

SCRIPT_PATH='/home/mini/workspace/dynamoChannel/gzip_password_compression_attack/victim' # the path of this script

#LD_PRELOAD=../../zlib/zlib/libz.so.1 taskset -c 3 ../../zlib/compress_path ../100_passwords/files/killer.txt
while true; do
	LD_PRELOAD=$SCRIPT_PATH/../../zlib/zlib/libz.so.1 taskset -c 3 $SCRIPT_PATH/../../zlib/compress_path $SCRIPT_PATH/../100_passwords/files/$1
done
