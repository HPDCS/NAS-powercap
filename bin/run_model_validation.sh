#!/bin/bash
ITERATIONS=2
OMP_THREADS=21

APPS="bt.W.x cg.B.x ft.A.x lu.B.x sp.B.x mg.C.x is.C.x ua.B.x"



export OMP_NUM_THREADS=$OMP_THREADS
for app in $APPS
do	
	for b in $(seq 1 $ITERATIONS)	
	do	
		echo "Running $app iteration $b..."
		./$app
	done
	echo "All $app runs completed."
done
