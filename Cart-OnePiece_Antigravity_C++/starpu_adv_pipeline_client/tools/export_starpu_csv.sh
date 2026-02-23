#!/bin/bash
export STARPU_FXT_PREFIX=/tmp
export STARPU_FXT_TRACE=1
# StarPU outputs profiler to fxt.
echo "Run StarPU app with STARPU_FXT_TRACE=1 to generate profiler, then convert via starpu_fxt_tool"
