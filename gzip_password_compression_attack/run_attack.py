#!/bin/python
import subprocess
import time
import random
from l1.parse_targetted_log import parse
from l1.parse_targetted_log import bits2words
import pandas as pd

filenames = ["killer.txt", "112233.txt"]

did_print_first_line = False

df = ""

for i in range(10):
	victim_file = random.choice(filenames)
	file = open('tmp.log', 'w')
	victim_process = subprocess.Popen(["./victim/run.sh", victim_file, "&"]) # runs on core #3
	time.sleep(1)
	attacker_process = subprocess.Popen(["taskset", "-c", "13", "./l1/targetted_attacker", "&"], stdout=file) # runs on core #13

	attacker_process.wait()
	victim_process.terminate()
	victim_process.wait()
	file.flush()
	file.close()

	file = open('tmp.log', 'r')
	o = parse(file.readlines())
	file.close()

	assert(len(o) == 2)

	if not did_print_first_line:
		did_print_first_line = True
		#output_line = ["Target"]
		columns = ["Target"] + [str(x) for x in list(range(len(o[0]) * 2))]
		#columns = [str(x) for x in list(range(len(o[0]) * 2))]
		df = pd.DataFrame(columns=columns)


	new_data = [victim_file] + [float(x) for x in (o[0] + o[1])]
	assert(len(new_data) == len(columns))
	num_rows = df.shape[0]
	df.loc[num_rows] = new_data
	num_rows = df.shape[0]

print(df)
df.to_parquet('dnn_data.parquet')
