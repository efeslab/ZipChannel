#!/bin/python
import lipsum
import itertools
import random
import string

output_bytes = 20000

paragraphs = lipsum.generate_paragraphs(1000).split('\n\n')

#for x, y in itertools.combinations(paragraphs, 2):
#	assert(x != y)

#formnr_diff_paragraphs in range(1, 100):
for nr_diff_paragraphs in (1,2,3,4,5):
	print(f"{nr_diff_paragraphs}")
	effective_paragraphs = paragraphs[0:nr_diff_paragraphs]
	assert(len(effective_paragraphs) == nr_diff_paragraphs)
	output = []
	while True:
		print(f"{output=}")
		effective_output_bytes = len("\n".join(output))
		print(f"{effective_output_bytes=}")
		if effective_output_bytes > output_bytes:
			break
		r = random.randint(0, nr_diff_paragraphs-1)
		par_to_add = effective_paragraphs[r]
		par_to_add = par_to_add[0:20]
		#par_to_add = ''.join(random.choices(string.ascii_lowercase + string.digits, k=20))
		output += [par_to_add]
	print(output)
	str_output = "\n".join(output)
	str_output = str_output[0:output_bytes]

	f = open(f"../microbenchmarks_testfiles/test_{nr_diff_paragraphs:05}.txt", "w")
	f.write(str_output)
	f.close()


