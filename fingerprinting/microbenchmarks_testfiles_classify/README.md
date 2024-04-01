to run:

```
rm -f ../microbenchmarks_testfiles/* ../microbenchmarks_testfiles_data/* && ./generate_testfiles.py
cd .. && time ./run_microbenchmarks.sh && cd -
./classify.py
```
