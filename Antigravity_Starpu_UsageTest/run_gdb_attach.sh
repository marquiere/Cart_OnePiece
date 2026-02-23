export RR_WORKERS_DEBUG=1
export STARPU_SCHED=rr_workers
export STARPU_NCPU=3
export STARPU_NCUDA=1
export STARPU_SCHED_LIB="$(pwd)/custom_sched_rr_workers/build/libstarpu_sched_rr_workers.so"

./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --frames 1 --mb 2 --infer-iters 1 > app_out.txt 2>&1 &
PID=$!
sleep 2

cat << 'EOF' > gdb_script.txt
thread apply all bt
quit
EOF

gdb -batch -x gdb_script.txt -p $PID > gdb_bt.txt 2>&1
kill -9 $PID
cat gdb_bt.txt
