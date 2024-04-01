#!/bin/python
import os

directory = '../microbenchmarks_testfiles_data'

for filename in os.listdir(directory):
	f = os.path.join(directory, filename)
# checking if it is a file
	if os.path.isfile(f):
		line = []

		file1 = open(f, 'r')
		Lines = file1.readlines()
		label = Lines[0]
		line += [label.split('/')[1][:-1]]

		try:
			line += [Lines[2][:-1]]
			line += [Lines[3][:-1]]
		except: # get here if the file did not catch any traces
			line += ["2 " * 10000]
			line += ["2 " * 10000]

		print(line)
		file1.close()
