#!/bin/bash
ITERATIONS=1
OMP_THREADS=21

MODES="10 11 12 15"
CAPS="45 50 60 70 75"
APPS="bt.B.x cg.B.x ft.A.x is.C.x mg.C.x sp.B.x lu.B.x"

export OMP_NUM_THREADS=$OMP_THREADS

for cap in $CAPS 
do
	python powercap_config_writer.py -power_limit $cap

	for mode in $MODES
	do
		python powercap_config_writer.py -heuristic_mode $mode
		for app in $APPS
		do
		        for b in $(seq 1 $ITERATIONS)   
		        do
		                echo "Running $app iteration $b..."
		                ./$app
		        done
		        echo "All $app runs completed."
		done
	done
done 