#!/bin/bash

max_cpu_id=39


#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
cpu=0
while [ $cpu -le $max_cpu_id ] 
        do
        cpufreq-set -c $cpu -f 2100000
        cpu=$[cpu+1]
done

