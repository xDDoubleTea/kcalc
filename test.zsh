#!/bin/zsh
make
sudo insmod kcalc.ko a=9223372036854775807 b=1 op="+" && sudo rmmod kcalc
sudo insmod kcalc.ko a=300 b=1 op="-" && sudo rmmod kcalc
sudo insmod kcalc.ko a=114514 b=67 op="*" && sudo rmmod kcalc
sudo insmod kcalc.ko a=23010 b=3314 op="/" && sudo rmmod kcalc
sudo insmod kcalc.ko a=100 b=1 op="+abb" && sudo rmmod kcalc
sudo insmod kcalc.ko a=100 b=0 op="/" && sudo rmmod kcalc
sudo insmod kcalc.ko a=1 b=0 op="%" && sudo rmmod kcalc
sudo insmod kcalc.ko a=2 b=3 op="^" && sudo rmmod kcalc
sudo insmod kcalc.ko a=2 b=67 op="^" && sudo rmmod kcalc
sudo insmod kcalc.ko a=199 b=193 op="^" && sudo rmmod kcalc
sudo insmod kcalc.ko a=1 b=-10 op="^" && sudo rmmod kcalc
