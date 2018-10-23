#!/bin/bash
sudo apt update
sudo apt install gcc
sudo apt install git
clone https://disanzo@bitbucket.org/disanzo/powercap.git
git clone https://github.com/deater/uarch-configure.git
cd powercap
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver 

#cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver 
#vim /boot/grub2/grub.cfg 
#export OMP_NUM_THREADS=1

#uarch-configure/
#cd rapl-read/
#make
#watch -n 1 ./rapl-read 

