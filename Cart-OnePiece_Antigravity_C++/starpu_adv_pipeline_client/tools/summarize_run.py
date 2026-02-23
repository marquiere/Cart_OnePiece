#!/usr/bin/env python3
import os
import sys
import glob
import json
import argparse
import subprocess
import datetime
import math

def parse_pids_env(run_dir):
    env_file = os.path.join(run_dir, "pids.env")
    data = {}
    if os.path.exists(env_file):
        with open(env_file, "r") as f:
            for line in f:
                line = line.strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    data[k] = v
    return data

def parse_meta(run_dir):
    meta_file = os.path.join(run_dir, "monitor_meta.txt")
    data = {}
    if os.path.exists(meta_file):
        with open(meta_file, "r") as f:
            for line in f:
                line = line.strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    data[k] = v
    return data

def parse_cmdline(run_dir):
    cmd_file = os.path.join(run_dir, "cmdline.txt")
    if os.path.exists(cmd_file):
        with open(cmd_file, "r") as f:
            return f.read().strip()
    return ""

def get_percentile(data, p):
    if not data:
        return 0.0
    s_data = sorted(data)
    idx = int(math.ceil(p * len(s_data))) - 1
    idx = max(0, min(len(s_data) - 1, idx))
    return s_data[idx]

def parse_cpu_log(log_path, method):
    if not os.path.exists(log_path):
        return []
    cpu_vals = []
    with open(log_path, "r") as f:
        lines = f.readlines()
        
    for line in lines:
        parts = line.split()
        if not parts:
            continue
        # pidstat format usually has %CPU
        # top format usually has %CPU
        if method == "pidstat":
            # skip headers and empty lines
            if "UID" in line or "Linux" in line or "Average" in line:
                continue
            # Usually 8th column is %CPU, check if a float
            try:
                # Find %CPU index from header if possible, else fallback to index 8
                # Format: Time UID PID %usr %system %guest %wait %CPU CPU Command
                if len(parts) >= 8:
                    # Some pidstat versions don't have %wait
                    for p in parts:
                        if "." in p: # Crude way to find the float percent
                            # we know %CPU is the highest float typically, or we just parse the columns
                            pass
                    # Let's just find the column named %CPU if we captured header
                    pass
            except Exception:
                pass
                
        # Simpler approach: find the first token containing a dot that parses as float, 
        # but top has %CPU and %MEM.
        if method == "top":
            if "PID" in line or "top -" in line or "Tasks:" in line or "Cpu(s):" in line:
                continue
            # PID USER PR NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND
            try:
                # 9th column is typically %CPU in default top batch mode
                if len(parts) >= 9:
                    cpu_vals.append(float(parts[8]))
            except ValueError:
                pass
        
        if method == "pidstat":
            # Time  UID  PID  %usr %system  %guest   %wait    %CPU   CPU  Command
            # index could be 7 or 8. Let's just look at the second to last numeric column
            try:
                sanitized_parts = [x.replace(',', '.') for x in parts]
                nums = [float(x) for x in sanitized_parts if "/" not in x and ":" not in x and x.replace('.', '', 1).isdigit()]
                if len(nums) >= 2:
                    cpu_vals.append(nums[-2]) # Usually %CPU is right before CPU core id
            except ValueError:
                pass

    return cpu_vals

def parse_gpu_log(log_path, method, server_pid, client_pid):
    server_mem = []
    client_mem = []
    if not os.path.exists(log_path):
        return server_mem, client_mem
        
    with open(log_path, "r") as f:
        for line in f:
            if method == "pmon":
                parts = line.split()
                if len(parts) >= 8 and parts[1] == str(server_pid):
                    try:
                        if parts[7] != "-": server_mem.append(float(parts[7]))
                    except: pass
                elif len(parts) >= 8 and parts[1] == str(client_pid):
                    try:
                        if parts[7] != "-": client_mem.append(float(parts[7]))
                    except: pass
            elif method == "compute-apps":
                if str(server_pid) in line:
                    try:
                        mem = float(line.split(',')[2].strip().split(' ')[0])
                        server_mem.append(mem)
                    except: pass
                elif str(client_pid) in line:
                    try:
                        mem = float(line.split(',')[2].strip().split(' ')[0])
                        client_mem.append(mem)
                    except: pass
    return server_mem, client_mem

def parse_pipeline_csv(run_dir):
    csv_path = os.path.join(run_dir, "pipeline_starpu.csv")
    latencies = []
    if os.path.exists(csv_path):
        with open(csv_path, "r") as f:
            header = True
            for line in f:
                if header:
                    header = False
                    continue
                parts = line.strip().split(',')
                if len(parts) >= 4:
                    try:
                        latencies.append(float(parts[3]))
                    except: pass
    return latencies

def parse_tasks_csv(run_dir):
    csv_path = os.path.join(run_dir, "starpu_tasks.csv")
    tasks = {"cl_preproc": [], "cl_infer_trt": [], "cl_post": []}
    if os.path.exists(csv_path):
        with open(csv_path, "r") as f:
            header = True
            for line in f:
                if header:
                    header = False
                    continue
                parts = line.strip().split(',')
                if len(parts) >= 8:
                    stage = parts[1]
                    try:
                        dur = float(parts[7])
                        if stage in tasks:
                            tasks[stage].append(dur)
                    except: pass
    return tasks

def main():
    parser = argparse.ArgumentParser(description="Summarize StarPU Pipeline Run")
    parser.add_argument("--run_dir", type=str, default="", help="Path to the runs/<timestamp> directory")
    args = parser.parse_args()

    run_dir = args.run_dir
    if not run_dir:
        runs = sorted(glob.glob("runs/*"))
        if not runs:
            print("No runs found.")
            return
        run_dir = runs[-1]
        
    print(f"Summarizing run in {run_dir}...")
    
    env_data = parse_pids_env(run_dir)
    meta_data = parse_meta(run_dir)
    cmdline = parse_cmdline(run_dir)
    
    server_pid = env_data.get("SERVER_PID", "")
    client_pid = env_data.get("CLIENT_PID", "")
    
    cpu_method = meta_data.get("CPU_METHOD", "unknown")
    gpu_method = meta_data.get("GPU_METHOD", "none")
    
    server_cpu = parse_cpu_log(os.path.join(run_dir, "monitor_cpu_server.log"), cpu_method)
    client_cpu = parse_cpu_log(os.path.join(run_dir, "monitor_cpu_client.log"), cpu_method)
    
    server_gpu_mem, client_gpu_mem = parse_gpu_log(os.path.join(run_dir, "monitor_gpu.log"), gpu_method, server_pid, client_pid)
    
    latencies = parse_pipeline_csv(run_dir)
    tasks = parse_tasks_csv(run_dir)
    
    # Extract host info
    try:
        cpu_model = subprocess.check_output("lscpu | grep 'Model name' | cut -d ':' -f2", shell=True).decode().strip()
    except:
        cpu_model = "Unknown"
    try:
        gpu_model = subprocess.check_output("nvidia-smi -L | head -n1", shell=True).decode().strip()
    except:
        gpu_model = "Unknown"
    
    summary = {
        "metadata": {
            "date_time": str(datetime.datetime.now()),
            "host": os.uname()[1],
            "cpu_model": cpu_model,
            "gpu_model": gpu_model,
            "server_cores": env_data.get("SERVER_CORES", "Unknown"),
            "client_cores": env_data.get("CLIENT_CORES", "Unknown"),
            "command": cmdline
        },
        "workload_split": {
            "server": {
                "avg_cpu_percent": sum(server_cpu)/len(server_cpu) if server_cpu else 0.0,
                "p95_cpu_percent": get_percentile(server_cpu, 0.95)
            },
            "client": {
                "avg_cpu_percent": sum(client_cpu)/len(client_cpu) if client_cpu else 0.0,
                "p95_cpu_percent": get_percentile(client_cpu, 0.95)
            },
            "gpu": {
                "method": gpu_method,
                "server_avg_used_gpu_memory_mb": sum(server_gpu_mem)/len(server_gpu_mem) if server_gpu_mem else 0.0,
                "client_avg_used_gpu_memory_mb": sum(client_gpu_mem)/len(client_gpu_mem) if client_gpu_mem else 0.0
            }
        },
        "starpu_tasks": {
            "preproc": {
                "avg_ms": sum(tasks["cl_preproc"])/len(tasks["cl_preproc"]) if tasks["cl_preproc"] else 0.0,
                "p95_ms": get_percentile(tasks["cl_preproc"], 0.95)
            },
            "infer": {
                "avg_ms": sum(tasks["cl_infer_trt"])/len(tasks["cl_infer_trt"]) if tasks["cl_infer_trt"] else 0.0,
                "p95_ms": get_percentile(tasks["cl_infer_trt"], 0.95)
            },
            "post": {
                "avg_ms": sum(tasks["cl_post"])/len(tasks["cl_post"]) if tasks["cl_post"] else 0.0,
                "p95_ms": get_percentile(tasks["cl_post"], 0.95)
            }
        },
        "pipeline_stats": {
            "end_to_end_latency_avg_ms": sum(latencies)/len(latencies) if latencies else 0.0,
            "end_to_end_latency_p95_ms": get_percentile(latencies, 0.95)
        }
    }
    
    with open(os.path.join(run_dir, "summary.json"), "w") as f:
        json.dump(summary, f, indent=4)
        
    with open(os.path.join(run_dir, "summary.txt"), "w") as f:
        f.write("=========================================\n")
        f.write(" Single-Machine Workload Split Report\n")
        f.write("=========================================\n\n")
        f.write("[Metadata]\n")
        f.write(f"Date/Time:      {summary['metadata']['date_time']}\n")
        f.write(f"Host:           {summary['metadata']['host']}\n")
        f.write(f"CPU Model:      {summary['metadata']['cpu_model']}\n")
        f.write(f"GPU Model:      {summary['metadata']['gpu_model']}\n")
        f.write(f"Server Cores:   {summary['metadata']['server_cores']}\n")
        f.write(f"Client Cores:   {summary['metadata']['client_cores']}\n")
        f.write(f"Command:        {summary['metadata']['command']}\n\n")
        
        f.write("[Workload Split]\n")
        f.write("Server Process:\n")
        f.write(f"  Avg CPU: {summary['workload_split']['server']['avg_cpu_percent']:.2f}%\n")
        f.write(f"  p95 CPU: {summary['workload_split']['server']['p95_cpu_percent']:.2f}%\n")
        f.write("Client Process:\n")
        f.write(f"  Avg CPU: {summary['workload_split']['client']['avg_cpu_percent']:.2f}%\n")
        f.write(f"  p95 CPU: {summary['workload_split']['client']['p95_cpu_percent']:.2f}%\n\n")
        
        f.write(f"[GPU Utilization] (Method: {gpu_method})\n")
        f.write(f"  Server Avg VRAM: {summary['workload_split']['gpu']['server_avg_used_gpu_memory_mb']:.2f} MB\n")
        f.write(f"  Client Avg VRAM: {summary['workload_split']['gpu']['client_avg_used_gpu_memory_mb']:.2f} MB\n\n")
        
        f.write("[StarPU Task Compute Timing]\n")
        f.write(f"  Preproc: Avg = {summary['starpu_tasks']['preproc']['avg_ms']:.2f} ms | p95 = {summary['starpu_tasks']['preproc']['p95_ms']:.2f} ms\n")
        f.write(f"  Infer:   Avg = {summary['starpu_tasks']['infer']['avg_ms']:.2f} ms | p95 = {summary['starpu_tasks']['infer']['p95_ms']:.2f} ms\n")
        f.write(f"  Post:    Avg = {summary['starpu_tasks']['post']['avg_ms']:.2f} ms | p95 = {summary['starpu_tasks']['post']['p95_ms']:.2f} ms\n\n")
        
        f.write("[Pipeline Stats]\n")
        f.write(f"  E2E Latency Avg: {summary['pipeline_stats']['end_to_end_latency_avg_ms']:.2f} ms\n")
        f.write(f"  E2E Latency p95: {summary['pipeline_stats']['end_to_end_latency_p95_ms']:.2f} ms\n\n")
        f.write("[Produced Files]\n")
        f.write(f"  {run_dir}/summary.txt\n")
        f.write(f"  {run_dir}/summary.json\n")
        f.write(f"  {run_dir}/pids.env\n")
        f.write(f"  {run_dir}/monitor_cpu_*.log\n")
        f.write(f"  {run_dir}/monitor_gpu.log\n")
        
    print(f"Summary generated at {os.path.join(run_dir, 'summary.txt')}")

if __name__ == "__main__":
    main()
