cat << 'EOF' > gdb_script.txt
run
thread apply all bt
quit
EOF
export RR_WORKERS_DEBUG=1
export STARPU_SCHED=rr_workers
export STARPU_NCPU=3
export STARPU_NCUDA=1
export STARPU_SCHED_LIB="$(pwd)/custom_sched_rr_workers/build/libstarpu_sched_rr_workers.so"
timeout 3 gdb -batch -x gdb_script.txt --args ./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --frames 1 --mb 2 --infer-iters 1 > gdb_out.txt 2>&1
cat gdb_out.txt
