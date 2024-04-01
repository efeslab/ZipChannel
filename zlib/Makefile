all: compress compress_path

compress: compress.cpp
	g++ compress.cpp -o compress -lz -g -ggdb

compress_path: compress_path.cpp
	g++ compress_path.cpp -o compress_path -lz -g -ggdb

run_gdb: compress zlib
	gdb --args env LD_PRELOAD=./zlib/libz.so.1 ./compress

run: compress zlib
	LD_PRELOAD=./zlib/libz.so.1 ./compress

zlib:
	make -C zlib

clean:
	rm -rf compress compress_path
