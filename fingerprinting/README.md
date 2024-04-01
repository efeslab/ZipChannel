run he attacker under debugger:
```
gdb --args env LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 ./FRattacker
```

or without debugger:
```
make && LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 ./FRattacker
```

here is a victim example:
```
LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 bzip2 -kf files/alice29.txt
```
