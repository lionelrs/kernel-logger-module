#!/bin/bash

make

sudo insmod klogger.ko
sudo chmod 666 /dev/klogger

make clean

echo "Hel1" > /dev/klogger
echo "Hel2" > /dev/klogger
echo "Hel3" > /dev/klogger
echo "Hel4" > /dev/klogger
echo "Hel5" > /dev/klogger
echo "Hel6" > /dev/klogger
echo "Hel7" > /dev/klogger

sudo rmmod klogger

sudo dmesg | tail -20