cloned from	https://github.com/DynamoRIO/dynamorio.git
At commit	b991249321d88117ddabd0bdf953e233928cefae

to compile:
```
cd dynamorio
cmake -B Build
cd Build
make
```

to run, go to `dynamorio/Build`:
```
./bin64/drrun -c ./api/bin/libmarina.so -- bzip2 tmp.txt -kf
```

From the same directory, running different aplications with the tool:
`zlib`:
```
make -j20 && time LD_PRELOAD=../../zlib/zlib/libz.so.1  ./bin64/drrun -c ./api/bin/libmarina.so -- ../../zlib/compress -fk |& less -R
```
`bzip2`, from the README, in order to compile as an `.so` run `make -f Makefile-libbz2_so` (I had to run `make -f Makefile-libbz2_so clean` first)
```
make -j20 && time LD_PRELOAD=../../bzip2-1.0.6/libbz2.so.1.0.6 ./bin64/drrun -c ./api/bin/libmarina.so -- bzip2 -fk ../../fingerprinting/files/alice29.txt |& less -R
```
```
make -j20 && time ./bin64/drrun -c ./api/bin/libmarina.so -- ../../ncompress/compress -fk ../../fingerprinting/files/alice29.txt |& less -R
```


## Analyze for `gzip_password_compression_attack `
```
make -j20 && time LD_PRELOAD=../../zlib/zlib/libz.so.1  ./bin64/drrun -c ./api/bin/libmarina.so -- ../../zlib/compress_path ../../gzip_password_compression_attack/100_passwords/files/killer.txt |& less -R
```
# ZipChannel-DynamoChannel
