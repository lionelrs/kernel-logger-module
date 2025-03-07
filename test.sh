#!/bin/bash

make

sudo insmod klogger.ko
sudo chmod 666 /dev/klogger

make clean

sudo rmmod klogger

sudo dmesg | tail -10