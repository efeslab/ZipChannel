all: flush_zlib

flush_zlib: flush_zlib.c cacheutils.h
	gcc flush_zlib.c -o flush_zlib -lz -O1 -lpthread

clean:
	rm -f flush_zlib
