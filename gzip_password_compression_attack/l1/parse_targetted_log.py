#!/bin/python
from collections import defaultdict
from functools import reduce
from operator import ior
from more_itertools import chunked


def parse(file):
	offset2bit = defaultdict(lambda: -1)
	lines = list(file)

	# finding what cache offsets are monitored
	for line in lines:
		x = line.split()
		if 'g_monitor_mask' in x:
			monitor_mask = int(x[2], 16)
			monitor_mask = bin(monitor_mask)[2:]
			for i, val in enumerate(reversed(monitor_mask)):
				assert(val in ('0', '1'))
				if val == '1':
					offset = i * 0x40
					assert(offset not in offset2bit)
					offset2bit[offset] = i
					assert(offset in offset2bit)
			break

	res = [[], []]
	assert(len(offset2bit) == 2)
	features = []
	for x in offset2bit:
		features += [x]

	for line in lines:
		if "iteration" not in line:
			continue
		data = line.split()
		data = data[2:]
		data = [int(x, 16) for x in data]
		for x in data:
			assert x in features
		if features[0] in data:
			res[0] += [1]
		else:
			res[0] += [0]

		if features[1] in data:
			res[1] += [1]
		else:
			res[1] += [0]
	
	return res

def bits2words(array, word_size = 20):
	res = []
	for subarray in array:
		subres = []
		for batch in chunked(subarray, word_size):
			x = batch
			x = [str(a) for a in x]
			x = "".join(x)
			x = int(x, 2)
			subres += [x]
		res += [subres]
	return res


if __name__ == "__main__":
	import fileinput
	res = parse(fileinput.input())
	#print(res)
	res = bits2words(res)
	print(res)
