#!/bin/python
import os
from collections import defaultdict
from statistics import median
import matplotlib.pyplot as plt

directory = "../microbenchmarks_testfiles_data"
filename2mainSort_cnt = defaultdict(lambda: [])
filename2fallbackSort_cnt = defaultdict(lambda: [])

for filename in os.listdir(directory):
	f = os.path.join(directory, filename)
	file = open(f, "r")
	Lines = file.readlines()

	#print(f"====================={f}")
	assert("microbenchmarks_testfiles" in Lines[0])
	testfile = Lines[0].split('/')[1].rstrip()
	#print(f"{testfile=}")

	assert("mainSort_cnt" in Lines[4])
	assert("fallbackSort_cnt" in Lines[5])
	mainSort_cnt = int(Lines[4].split("=")[1])
	fallbackSort_cnt = int(Lines[5].split("=")[1])
#	print(f"{mainSort_cnt=}")
#	print(f"{fallbackSort_cnt=}")

	filename2mainSort_cnt[testfile] += [mainSort_cnt]
	filename2fallbackSort_cnt[testfile] += [fallbackSort_cnt]

print(f"{filename2mainSort_cnt=}")
print(f"{filename2fallbackSort_cnt=}")

plot_y = []

for k in sorted(filename2mainSort_cnt):
	print(k)
	msk = filename2mainSort_cnt[k]
	fbk = filename2fallbackSort_cnt[k]
	print(f"mainsort avg: {sum(msk)/len(msk)}, min: {min(msk)}, max: {max(msk)}, median: {median(msk)}, percent_uner_threshold: {len([x for x in msk if x < 145])/len(msk)}")
	print(f"fallbacksort avg: {sum(fbk)/len(fbk)}, min: {min(fbk)}, max: {max(fbk)}")
	plot_y += [sum(fbk)/len(fbk)]

plt.ylabel('#misses on fallbackSort')
plt.xlabel('repetitivness')
#plt.yscale("log") 

plt.plot(range(1,6), plot_y) # TODO: make this not hard-coded. Second parameter used to be 100
plt.savefig("plot.png")
plt.savefig(f"microbench_clasify.pdf", format="pdf", bbox_inches="tight")
