cat << 'PYEOF' > gdb_script.py
import gdb
import threading
import time
import os
import signal

def interrupt_gdb():
    time.sleep(2)
    os.kill(os.getpid(), signal.SIGINT)

threading.Thread(target=interrupt_gdb).start()
try:
    gdb.execute("run")
except Exception:
    pass

gdb.execute("set pagination off")
gdb.execute("thread apply all bt")
gdb.execute("quit")
PYEOF

export RR_WORKERS_DEBUG=1
export STARPU_SCHED=rr_workers
export STARPU_NCPU=3
export STARPU_NCUDA=1
export STARPU_SCHED_LIB="$(pwd)/custom_sched_rr_workers/build/libstarpu_sched_rr_workers.so"

gdb -q -x gdb_script.py --args ./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --frames 1 --mb 2 --infer-iters 1 > gdb_out.txt 2>&1
cat gdb_out.txt
