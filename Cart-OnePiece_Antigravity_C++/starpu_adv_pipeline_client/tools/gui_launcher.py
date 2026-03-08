import os
import subprocess
import threading
import sys
import tkinter as tk
import customtkinter as ctk

# Ensure working directory is set to project root safely
PROJ_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
os.chdir(PROJ_ROOT)

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class LauncherApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("Cart_OnePiece: StarPU Pipeline Launcher")
        self.geometry("900x750")
        
        # Grid layout
        self.grid_columnconfigure(0, weight=1)
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(3, weight=1)

        # ----------------------------------------------------------------------
        # Section 1: SIMULATION
        # ----------------------------------------------------------------------
        self.frame_sim = ctk.CTkFrame(self)
        self.frame_sim.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_sim, text="Section 1 — Simulation", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        
        self.entry_host = self.create_input(self.frame_sim, "Host:", "127.0.0.1")
        self.entry_port = self.create_input(self.frame_sim, "Port:", "2000")
        self.entry_map = self.create_input(self.frame_sim, "Map (Optional):", "")
        self.entry_frames = self.create_input(self.frame_sim, "Frames:", "200")

        # ----------------------------------------------------------------------
        # Section 2: SCHEDULER
        # ----------------------------------------------------------------------
        self.frame_sched = ctk.CTkFrame(self)
        self.frame_sched.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_sched, text="Section 2 — Scheduler", font=ctk.CTkFont(weight="bold")).pack(pady=5)

        self.sched_var = tk.StringVar(value="dmda")
        self.sched_dropdown = ctk.CTkOptionMenu(self.frame_sched, variable=self.sched_var, 
                                                values=["dmda", "rr_workers", "ws", "eager", "custom"])
        self.sched_dropdown.pack(pady=5, padx=10, fill="x")

        self.sweep_var = tk.BooleanVar(value=False)
        self.sweep_cb = ctk.CTkCheckBox(self.frame_sched, text="Run scheduler sweep (All modes)", variable=self.sweep_var)
        self.sweep_cb.pack(pady=5, padx=10, anchor="w")

        # ----------------------------------------------------------------------
        # Section 3: CAMERA DEPLOYMENT
        # ----------------------------------------------------------------------
        self.frame_cam = ctk.CTkFrame(self)
        self.frame_cam.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_cam, text="Section 3 — Camera Deployment", font=ctk.CTkFont(weight="bold")).pack(pady=5)

        self.cam_var = tk.StringVar(value="8 cameras")
        self.cam_dropdown = ctk.CTkOptionMenu(self.frame_cam, variable=self.cam_var, 
                                              values=["2 cameras", "4 cameras", "8 cameras"])
        self.cam_dropdown.pack(pady=5, padx=10, fill="x")
        self.cam_dropdown.configure(state="disabled")
        ctk.CTkLabel(self.frame_cam, text="(backend camera selection not exposed yet)", text_color="gray").pack()

        # ----------------------------------------------------------------------
        # Section 4: STARPU ANALYSES
        # ----------------------------------------------------------------------
        self.frame_starpu = ctk.CTkFrame(self)
        self.frame_starpu.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_starpu, text="Section 4 — StarPU Analyses", font=ctk.CTkFont(weight="bold")).pack(pady=5)

        # Traces are mostly generated automatically by verify_profiling.sh
        # We only expose what is controllable
        self.mem_var = tk.BooleanVar(value=True)
        ctk.CTkCheckBox(self.frame_starpu, text="Memory / Bus Stats (--memory)", variable=self.mem_var).pack(pady=2, padx=10, anchor="w")

        self.display_var = tk.BooleanVar(value=True)
        ctk.CTkCheckBox(self.frame_starpu, text="Show Semantic Viewer (--display)", variable=self.display_var).pack(pady=2, padx=10, anchor="w")

        ctk.CTkLabel(self.frame_starpu, text="APEX Mode:").pack(pady=(5,0), padx=10, anchor="w")
        self.apex_var = tk.StringVar(value="all")
        self.apex_dropdown = ctk.CTkOptionMenu(self.frame_starpu, variable=self.apex_var, 
                                               values=["off", "all", "gtrace", "gtrace-tasks", "taskgraph"])
        self.apex_dropdown.pack(pady=2, padx=10, fill="x")

        ctk.CTkLabel(self.frame_starpu, text="(FxT, Graphviz, Codelet, StarVZ always run)", text_color="gray").pack(pady=2)

        # ----------------------------------------------------------------------
        # Section 5: GENERIC ANALYSES
        # ----------------------------------------------------------------------
        self.frame_gen = ctk.CTkFrame(self)
        self.frame_gen.grid(row=2, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_gen, text="Section 5 — Generic Analyses", font=ctk.CTkFont(weight="bold")).pack(pady=5)

        self.dataset_var = tk.BooleanVar(value=False)
        ctk.CTkCheckBox(self.frame_gen, text="Run Dataset Generation (Full Demo)", variable=self.dataset_var, command=self.on_dataset_toggle).pack(pady=2, padx=10, anchor="w")
        
        self.summary_var = tk.BooleanVar(value=True)
        cb = ctk.CTkCheckBox(self.frame_gen, text="Summarize Metrics", variable=self.summary_var)
        cb.pack(pady=2, padx=10, anchor="w")
        cb.configure(state="disabled")
        ctk.CTkLabel(self.frame_gen, text="(Summary implicitly ran by wrappers)", text_color="gray").pack(pady=0)

        # ----------------------------------------------------------------------
        # Section 6: EXECUTION
        # ----------------------------------------------------------------------
        self.frame_exec = ctk.CTkFrame(self)
        self.frame_exec.grid(row=2, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_exec, text="Section 6 — Execution", font=ctk.CTkFont(weight="bold")).pack(pady=5)

        self.btn_preview = ctk.CTkButton(self.frame_exec, text="Preview Command", command=self.preview_cmd)
        self.btn_preview.pack(pady=5, padx=10, fill="x")

        self.btn_run = ctk.CTkButton(self.frame_exec, text="Run Target Sequence", fg_color="green", hover_color="darkgreen", command=self.run_pipeline)
        self.btn_run.pack(pady=5, padx=10, fill="x")

        self.btn_stop = ctk.CTkButton(self.frame_exec, text="Stop Any Running Process", fg_color="red", hover_color="darkred", command=self.stop_process)
        self.btn_stop.pack(pady=5, padx=10, fill="x")

        # ----------------------------------------------------------------------
        # Console Window
        # ----------------------------------------------------------------------
        self.console = ctk.CTkTextbox(self, wrap="word", font=("Courier", 12))
        self.console.grid(row=3, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")
        
        self.current_process = None
        self.log_msg("GUI Initialized. Working Directory: " + PROJ_ROOT)

    def create_input(self, parent, label_text, default_val):
        frame = ctk.CTkFrame(parent, fg_color="transparent")
        frame.pack(fill="x", padx=10, pady=2)
        ctk.CTkLabel(frame, text=label_text, width=100, anchor="w").pack(side="left")
        entry = ctk.CTkEntry(frame)
        entry.pack(side="right", fill="x", expand=True)
        entry.insert(0, default_val)
        return entry

    def on_dataset_toggle(self):
        if self.dataset_var.get():
            self.sweep_cb.deselect()
            self.sweep_cb.configure(state="disabled")
            self.sched_dropdown.configure(state="disabled")
        else:
            self.sweep_cb.configure(state="normal")
            self.sched_dropdown.configure(state="normal")
            
    def log_msg(self, text):
        self.console.insert("end", text + "\n")
        self.console.see("end")

    def build_command(self):
        frames = self.entry_frames.get().strip() or "200"
        
        cmd = []
        host = self.entry_host.get().strip()
        port = self.entry_port.get().strip()
        mem = "--memory" if self.mem_var.get() else ""
        disp = "--display" if self.display_var.get() else ""
        apex = f"--apex-mode {self.apex_var.get()}" if self.apex_var.get() != "off" else ""
        
        if self.dataset_var.get():
            cmd = ["./tools/run_full_demo.sh", "--frames", frames]
            if self.mem_var.get(): cmd.append("--memory")
            if self.display_var.get(): cmd.append("--display")
            return " ".join(cmd)
            
        elif self.sweep_var.get():
            cmd = ["./tools/run_verify_demo.sh", "--frames", frames]
            if self.mem_var.get(): cmd.append("--memory")
            if self.display_var.get(): cmd.append("--display")
            if self.apex_var.get() != "off": 
                cmd.append("--apex-mode")
                cmd.append(self.apex_var.get())
            return " ".join(cmd)
            
        else:
            sched = self.sched_var.get()
            seq = (
                f"./tools/run_server.sh --port {port} && "
                f"sleep 12 && "
                f"./tools/verify_profiling.sh --frames {frames} --sched {sched} {mem} {disp} {apex} ; "
                f"./tools/kill_server.sh"
            )
            return seq

    def preview_cmd(self):
        cmd = self.build_command()
        self.log_msg(f"\n[Preview] Scheduled Command:\n{cmd}\n")

    def stop_process(self):
        self.log_msg("\n[!] Force killing safely... (pkill CarlaUE4 & StarPU)")
        subprocess.run(["pkill", "-9", "-f", "CarlaUE4"], capture_output=True)
        subprocess.run(["pkill", "-9", "-f", "pipeline_starpu"], capture_output=True)
        subprocess.run(["pkill", "-9", "-f", "dataset_sanity"], capture_output=True)
        subprocess.run(["./tools/kill_server.sh"], capture_output=True)
        if self.current_process:
            try:
                self.current_process.terminate()
            except:
                pass
            self.current_process = None
        self.log_msg("[!] Stopped.\n")

    def run_pipeline(self):
        if self.current_process and self.current_process.poll() is None:
            self.log_msg("A process is already running! Stop it first.")
            return

        cmd = self.build_command()
        self.log_msg(f"\n====================================\n[Exec] Starting Subprocess Route:\n{cmd}\n")

        # Clear output box
        self.console.delete("1.0", "end")
        self.log_msg(f"[Exec] Initializing sequence:\n{cmd}\n")
        
        threading.Thread(target=self._exec_thread, args=(cmd,), daemon=True).start()

    def _exec_thread(self, cmd):
        try:
            # We strictly route through Bash so ampersand and sleep chains evaluate
            self.current_process = subprocess.Popen(
                ["bash", "-c", cmd],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1 # Line buffered
            )

            for line in self.current_process.stdout:
                # Need to use after() to thread-safely update tkinter text
                # It evaluates smoothly for Python 3.10+
                self.console.after(0, self.log_msg, line.strip('\n'))

            self.current_process.wait()
            self.console.after(0, self.log_msg, f"\n[Done] Process exited with code {self.current_process.returncode}\n====================================")
        except Exception as e:
            self.console.after(0, self.log_msg, f"\n[Error] Failed to execute safely: {str(e)}")
        finally:
            self.current_process = None
            # Enforce clean shutdown sequentially after failure
            subprocess.run(["./tools/kill_server.sh"], capture_output=True)

if __name__ == "__main__":
    app = LauncherApp()
    app.mainloop()
