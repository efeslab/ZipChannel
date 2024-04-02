This attack contains code for both fingerprinting attacks in ZipChannel. At a high level. to run the attack, we need to invoke the attacker that probes the cache alongside the victim and collect the traces. We note that [`FRattacker.c`](FRattacker) has hard-coded addresses where it expects the cache lines it monitors to be loaded. The file contains a comment with sample outputs from both TaintsChannel and GDB that help compute the correct addresses for your system.


## Some useful command line argumens to manually run the attack

run he attacker under debugger:
```
gdb --args env LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 ./FRattacker
```

or without debugger:
```
make && LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 ./FRattacker
```

an example for how to run the victim with the Bzip2 version in the parent directory.
```
LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 bzip2 -kf files/alice29.txt
```

## Classification of microbenchmark
See instructions in [`microbenchmarks_testfiles_classify`](microbenchmarks_testfiles_classify). Note: the scripts there use [`run_microbenchmarks.sh`](run_microbenchmarks.sh) from the current directory.

## Classification of Brotli test files
Use [`run_brotli_testfiles.sh`] to orcestrate the attack, i.e., run the attacker and the victm. Create the directory `brotli_testfiles_data` and store traces inside it.

To parse the traces, inside the directory [`brotli_testfiles_classify`](brotli_testfiles_classify), the script [`brotli_testfiles_classify/parse_data.py`](brotli_testfiles_classify/parse_data.py) collects all the data into a single file that you can then feed into the DNN classifier in [`brotli_testfiles_classify/FFNN_Tabular_Multiclass_Classification.ipynb`](brotli_testfiles_classify/FFNN_Tabular_Multiclass_Classification.ipynb).
Note that we use Google Colab to run the notebook, and use the Google Drive connectivity to feed files into it.
