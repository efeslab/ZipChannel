This directory contains the microbenchmark experiment from ZipChannel, where we classify files with different repetitiveness factor based on the way Bzip2 compresses them.

To run the data collection, we first generate the files using the script [`generate_testfiles.py`](generate_testfiles.py). We then use [`../run_microbenchmarks.sh`](../run_microbenchmarks.sh) to run the attacker alongside the victim and collect traces into `../microbenchmarks_testfiles_data`
```
rm -f ../microbenchmarks_testfiles/* ../microbenchmarks_testfiles_data/* && ./generate_testfiles.py
cd .. && time ./run_microbenchmarks.sh && cd -
```

We then aggregate the data using [`parse_data.py](parse_data.py), which aggregates all the data from `../microbenchmarks_testfiles_data` into a single CSV file, which the Jupither notebook in [`FFNN_Tabular_Multiclass_Classification.ipynb`]. Not that we use Google Colab to run the notebook, and it takes the file with the name `microbench1.log` from the Google Drive folder that is specified in the notebook, and also outputs data into the same folder.

Additionally, is not neccesary for the attack, but [`classify.py`](classify.py) helps to visualize the data.
