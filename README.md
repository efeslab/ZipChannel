# ZipChannel
This repository contains artifacts for the ZipChannel paper, which be found [here](https://dsn2024uq.github.io/Proceedings/pdfs/DSN2024-6rvE3SSpzFYmysif75Dkid/410500a223/410500a223.pdf). The repository contains the source code of TaintChannel and the code used for the fingerprinting attacks.
The code for the SGX attack from the paper is in these two repositories:
* User code: https://github.com/efeslab/ZipChannel_SGX_attack
* Kernel Driver: https://github.com/efeslab/ZipChannel_linux-sgx-driver

## TaintChannel
The code in the directory [`dynamorio`](dynamorio) is originally from:  
cloned from	[https://github.com/DynamoRIO/dynamorio.git]  
At commit	`b991249321d88117ddabd0bdf953e233928cefae`

The source code for TaintChannel is in: [`dynamorio/api/samples/marina.cpp`](dynamorio/api/samples/marina.cpp)

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

## Code we tested TaintChannel on
We include the target libraries in submodules, to initialize them run:
```
git submodule init
git submodule update
```

Note: while [`ncompress`](ncompress) and [`bzip2-1.0.6`](bzip2-1.0.6) contain executables, because [`zlib`](zlib) is a library it amso contains a small code to run it as an executable.

[`aes_T-table`](aes_T-table) contains main code that invokes the vulnerable path in the AES library already installed on the syetem.

## 
From the directory [`dynamorio`](dynamorio), here are command lines for running different aplications with the TaintChannel:
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

## Sample output
We include directories with sample output from TaintChannel. Each of these has its own README.

These directories contain the sample outputs from TaintChannel that were used for generating the figures:  
[`fig_bzip2_best_gadget`](fig_bzip2_best_gadget)  
[`fig_ncompress_best_gadget`](fig_ncompress_best_gadget)  
[`fig_zlib_best_gadget`](fig_zlib_best_gadget)

This is a simple example that is not included in the paper, but can be a good starting point
[`fig_example`](fig_example).
