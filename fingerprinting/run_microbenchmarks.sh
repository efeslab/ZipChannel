#!/bin/bash
make

filenames=(microbenchmarks_testfiles/*)
filenames_size=${#filenames[@]}
#nr_iteration=10000
nr_iteration=5000

mkdir microbenchmarks_testfiles_data

for i in `seq $nr_iteration`; do
	echo "============================="
	data_filename=$RANDOM.txt
	rm -f microbenchmarks_testfiles_data/$data_filename # make sure there are no duplicates

	index=$(($RANDOM % $filenames_size))
	filename=${filenames[$index]}
	echo iteration $i, $filename
	echo $filename >> microbenchmarks_testfiles_data/$data_filename
	
	LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 taskset -c 1 timeout 5s ./FRattacker >> microbenchmarks_testfiles_data/$data_filename &
	sleep 1
	
	LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 taskset -c 5 bzip2 -kf $filename
	wait

	rm $filename.bz2
	echo done
done

echo done!
