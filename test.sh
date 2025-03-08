#!/bin/bash

make

sudo insmod klogger.ko
sudo chmod 666 /dev/klogger

make clean


for i in {1..7}
do
    echo "Hello$i" > /dev/klogger
done


cat /dev/klogger

# Expected output: 
# Hello4
# Hello5
# Hello6
# Hello7

sudo rmmod klogger