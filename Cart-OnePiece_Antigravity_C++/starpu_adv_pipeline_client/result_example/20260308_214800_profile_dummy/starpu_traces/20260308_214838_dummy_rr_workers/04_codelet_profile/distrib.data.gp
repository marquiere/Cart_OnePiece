#!/usr/bin/gnuplot -persist
set term postscript eps enhanced color
set logscale x
set logscale y
set output "distrib.data.eps"
set key top left
set xlabel "Total data size"
set ylabel "Execution time (ms)"
plot	