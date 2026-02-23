set terminal eps
set output "cholesky.eps"
set key top left
set xlabel "size"
set ylabel "GFlop/s"
plot \
'cholesky.output' using 1:2 with lines title 'dmdas' \
, 'cholesky.output' using 1:3 with lines title 'modular-dmdas' \
, 'cholesky.output' using 1:4 with lines title 'modular-heft2' \
, 'cholesky.output' using 1:5 with lines title 'modular-heft' \
, 'cholesky.output' using 1:6 with lines title 'modular-heft-prio' \
, 'cholesky.output' using 1:7 with lines title 'modular-heteroprio' \
, 'cholesky.output' using 1:8 with lines title 'dmdap' \
, 'cholesky.output' using 1:9 with lines title 'dmdar' \
, 'cholesky.output' using 1:10 with lines title 'dmda' \
, 'cholesky.output' using 1:11 with lines title 'dmdasd' \
, 'cholesky.output' using 1:12 with lines title 'modular-dmdap' \
, 'cholesky.output' using 1:13 with lines title 'modular-dmdar' \
, 'cholesky.output' using 1:14 with lines title 'modular-dmda' \
, 'cholesky.output' using 1:15 with lines title 'prio' \
, 'cholesky.output' using 1:16 with lines title 'lws' \
