#!/bin/bash

max_cpu_id=31


#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
cpu=0
while [ $cpu -le $max_cpu_id ] 
        do
        cpufreq-set -c $cpu -f 2000000
        cpu=$[cpu+1]
done

