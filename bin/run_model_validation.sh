ITERATIONS=2

export OMP_NUM_THREADS=21
for b in $(seq 1 $ITERATIONS)	
do	
	echo "Running ..."
	 ./bt.S.x
	
done

echo "Completed."
