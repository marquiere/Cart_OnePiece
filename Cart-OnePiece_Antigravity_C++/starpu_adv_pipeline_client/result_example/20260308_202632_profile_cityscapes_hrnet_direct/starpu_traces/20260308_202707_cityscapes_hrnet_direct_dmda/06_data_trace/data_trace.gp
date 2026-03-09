#!/usr/bin/gnuplot -persist

set term postscript eps enhanced color
set output "./data_trace.eps"
set title "Data trace"
set logscale x
set logscale y
set xlabel "data size (B)"
set ylabel "tasks size (ms)"
plot 
