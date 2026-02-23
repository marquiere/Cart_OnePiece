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
