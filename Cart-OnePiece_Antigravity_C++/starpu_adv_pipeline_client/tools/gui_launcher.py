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
        self.geometry("900x800")
        
        # Grid layout
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)

        # ----------------------------------------------------------------------
        # Tab View
        # ----------------------------------------------------------------------
        self.tabview = ctk.CTkTabview(self)
        self.tabview.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        
        self.tab_sim = self.tabview.add("Simulation & Engine")
        self.tab_env = self.tabview.add("Environment & Output")
        self.tab_starpu = self.tabview.add("StarPU & Analyzers")
        self.tab_visual = self.tabview.add("Camera & Output")
        
        # --- TAB 1: Simulation ---
        self.tab_sim.grid_columnconfigure(0, weight=1)
        self.tab_sim.grid_columnconfigure(1, weight=1)

        # Simulation Sub-Frame
        frm_sim = ctk.CTkFrame(self.tab_sim)
        frm_sim.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_sim, text="Simulation Config", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        self.entry_host = self.create_input(frm_sim, "CARLA Host:", "127.0.0.1")
        self.entry_port = self.create_input(frm_sim, "CARLA Port:", "2000")
        self.entry_frames = self.create_input(frm_sim, "Total Frames:", "200")
        self.entry_fps = self.create_input(frm_sim, "Target FPS:", "20")

        # Engine Sub-Frame
        frm_engine = ctk.CTkFrame(self.tab_sim)
        frm_engine.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_engine, text="TensorRT Engine", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        self.engine_var = tk.StringVar(value="models/dummy.engine")
        self.engine_dropdown = ctk.CTkOptionMenu(frm_engine, variable=self.engine_var, 
                                                 values=["models/dummy.engine", "models/deeplabv3_mobilenet.engine", "models/fcn_resnet50.engine", "models/carla_resnet50_best.engine"])
        self.engine_dropdown.pack(pady=5, padx=10, fill="x")
        self.entry_custom_engine = self.create_input(frm_engine, "Custom Path:", "")

        # --- TAB 1.5: Environment ---
        self.tab_env.grid_columnconfigure(0, weight=1)
        self.tab_env.grid_columnconfigure(1, weight=1)

        # Output Sub-Frame
        frm_out = ctk.CTkFrame(self.tab_env)
        frm_out.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_out, text="Output Control", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        self.entry_out_dir = self.create_input(frm_out, "Custom Output Dir:", "")
        ctk.CTkLabel(frm_out, text="(Leave blank for default: runs/<TIMESTAMP>...)", text_color="gray").pack()

        # Traffic Sub-Frame
        frm_traffic = ctk.CTkFrame(self.tab_env)
        frm_traffic.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_traffic, text="Map & Traffic Generation", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        
        self.map_var = tk.StringVar(value="Town03_Opt")
        maps = ["Town01_Opt", "Town02_Opt", "Town03_Opt", "Town04_Opt", "Town05_Opt", "Town06_Opt", "Town07_Opt", "Town10HD_Opt"]
        self.map_dropdown = ctk.CTkOptionMenu(frm_traffic, variable=self.map_var, values=maps)
        self.map_dropdown.pack(pady=5, padx=10, fill="x")
        
        self.entry_veh = self.create_input(frm_traffic, "Vehicles Spawn:", "30")
        self.entry_ped = self.create_input(frm_traffic, "Pedestrians Spawn:", "10")

        # --- TAB 2: StarPU ---
        self.tab_starpu.grid_columnconfigure(0, weight=1)
        self.tab_starpu.grid_columnconfigure(1, weight=1)

        # Sched Sub-Frame
        frm_sched = ctk.CTkFrame(self.tab_starpu)
        frm_sched.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_sched, text="StarPU Parameters", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        
        # All available schedulers
        schedulers = ["dmda", "eager", "prio", "random", "ws", "lws", "dm", "dmdap", "dmdar", "dmdas", "dmdasd", "peager", "heteroprio", "rr_workers"]
        self.sched_var = tk.StringVar(value="dmda")
        self.sched_dropdown = ctk.CTkOptionMenu(frm_sched, variable=self.sched_var, values=schedulers)
        self.sched_dropdown.pack(pady=5, padx=10, fill="x")

        self.sweep_var = tk.BooleanVar(value=False)
        self.sweep_cb = ctk.CTkCheckBox(frm_sched, text="Run Scheduler Sweep (4 Core Scheds)", variable=self.sweep_var)
        self.sweep_cb.pack(pady=5, padx=10, anchor="w")

        self.entry_inflight = self.create_input(frm_sched, "In-flight tasks:", "2")
        self.entry_cpu = self.create_input(frm_sched, "CPU Workers:", "8")

        # Analysis Sub-Frame
        frm_analysis = ctk.CTkFrame(self.tab_starpu)
        frm_analysis.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_analysis, text="Trace Analyzers", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        
        self.mem_var = tk.BooleanVar(value=True)
        ctk.CTkCheckBox(frm_analysis, text="Record Memory / Bus Stats", variable=self.mem_var).pack(pady=2, padx=10, anchor="w")

        ctk.CTkLabel(frm_analysis, text="APEX Profiling Mode:").pack(pady=(5,0), padx=10, anchor="w")
        self.apex_var = tk.StringVar(value="off")
        self.apex_dropdown = ctk.CTkOptionMenu(frm_analysis, variable=self.apex_var, 
                                               values=["off", "all", "gtrace", "gtrace-tasks", "taskgraph"])
        self.apex_dropdown.pack(pady=2, padx=10, fill="x")

        # --- TAB 3: Visual & Format ---
        self.tab_visual.grid_columnconfigure(0, weight=1)
        self.tab_visual.grid_columnconfigure(1, weight=1)

        frm_cam = ctk.CTkFrame(self.tab_visual)
        frm_cam.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_cam, text="Resolution & Format", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        self.entry_w = self.create_input(frm_cam, "Input W:", "800")
        self.entry_h = self.create_input(frm_cam, "Input H:", "600")
        self.entry_out_w = self.create_input(frm_cam, "Model W:", "512")
        self.entry_out_h = self.create_input(frm_cam, "Model H:", "256")
        
        self.bgra_var = tk.BooleanVar(value=True)
        ctk.CTkCheckBox(frm_cam, text="Force --assume_bgra 1", variable=self.bgra_var).pack(pady=5, padx=10, anchor="w")

        frm_disp = ctk.CTkFrame(self.tab_visual)
        frm_disp.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(frm_disp, text="Display / Dataset", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        
        self.display_var = tk.BooleanVar(value=True)
        ctk.CTkCheckBox(frm_disp, text="Live SDL2 Semantic Viewer (--display)", variable=self.display_var).pack(pady=5, padx=10, anchor="w")

        self.nopred_var = tk.BooleanVar(value=False)
        ctk.CTkCheckBox(frm_disp, text="Skip Inference (--no_pred)", variable=self.nopred_var).pack(pady=5, padx=10, anchor="w")

        # ----------------------------------------------------------------------
        # Action Buttons (Always Visible)
        # ----------------------------------------------------------------------
        frm_actions = ctk.CTkFrame(self)
        frm_actions.grid(row=1, column=0, padx=10, pady=5, sticky="ew")
        frm_actions.grid_columnconfigure(0, weight=1)
        frm_actions.grid_columnconfigure(1, weight=1)

        self.dataset_var = tk.BooleanVar(value=False)
        self.cb_dataset = ctk.CTkCheckBox(frm_actions, text="Run Dataset Extraction Mode", variable=self.dataset_var, command=self.on_dataset_toggle)
        self.cb_dataset.grid(row=0, column=0, padx=5, pady=10, sticky="e")

        self.split_var = tk.BooleanVar(value=False)
        self.cb_split = ctk.CTkCheckBox(frm_actions, text="Multi-Process Splitting", variable=self.split_var)
        self.cb_split.grid(row=0, column=1, padx=5, pady=10, sticky="w")

        frm_btns = ctk.CTkFrame(frm_actions, fg_color="transparent")
        frm_btns.grid(row=1, column=0, columnspan=2, pady=(0, 10))

        ctk.CTkButton(frm_btns, text="Preview Command", command=self.preview_cmd).pack(side="left", padx=10)
        ctk.CTkButton(frm_btns, text="Run Pipeline", fg_color="green", hover_color="darkgreen", command=self.run_pipeline).pack(side="left", padx=10)
        ctk.CTkButton(frm_btns, text="Force Stop", fg_color="red", hover_color="darkred", command=self.stop_process).pack(side="left", padx=10)

        # ----------------------------------------------------------------------
        # Console Window
        # ----------------------------------------------------------------------
        self.console = ctk.CTkTextbox(self, wrap="word", font=("Courier", 12))
        self.console.grid(row=2, column=0, padx=10, pady=10, sticky="nsew")
        self.grid_rowconfigure(2, weight=1)
        
        self.current_process = None
        self.log_msg("GUI v2 Initialized. Integrated all arguments.")

    def create_input(self, parent, label_text, default_val):
        frame = ctk.CTkFrame(parent, fg_color="transparent")
        frame.pack(fill="x", padx=10, pady=2)
        ctk.CTkLabel(frame, text=label_text, width=110, anchor="w").pack(side="left")
        entry = ctk.CTkEntry(frame)
        entry.pack(side="right", fill="x", expand=True)
        entry.insert(0, default_val)
        return entry

    def on_dataset_toggle(self):
        if self.dataset_var.get():
            self.sweep_cb.deselect()
            self.sweep_cb.configure(state="disabled")
            self.sched_dropdown.configure(state="disabled")
            self.split_var.set(False)
            self.cb_split.configure(state="disabled")
        else:
            self.sweep_cb.configure(state="normal")
            self.sched_dropdown.configure(state="normal")
            self.cb_split.configure(state="normal")
            
    def log_msg(self, text):
        self.console.insert("end", text + "\n")
        self.console.see("end")

    def build_command(self):
        cmd = ["./tools/run_pipeline.sh"]
        
        cmd.extend(["--frames", self.entry_frames.get().strip() or "200"])
        cmd.extend(["--host", self.entry_host.get().strip() or "127.0.0.1"])
        cmd.extend(["--port", self.entry_port.get().strip() or "2000"])
        cmd.extend(["--fps", self.entry_fps.get().strip() or "20"])
        cmd.extend(["--w", self.entry_w.get().strip() or "800"])
        cmd.extend(["--h", self.entry_h.get().strip() or "600"])
        cmd.extend(["--out_w", self.entry_out_w.get().strip() or "512"])
        cmd.extend(["--out_h", self.entry_out_h.get().strip() or "256"])
        cmd.extend(["--inflight", self.entry_inflight.get().strip() or "2"])
        cmd.extend(["--cpu_workers", self.entry_cpu.get().strip() or "8"])
        
        out_dir = self.entry_out_dir.get().strip()
        if out_dir:
            cmd.extend(["--out_dir", out_dir])
            
        cmd.extend(["--map", self.map_var.get()])
        cmd.extend(["--vehicles", self.entry_veh.get().strip() or "30"])
        cmd.extend(["--pedestrians", self.entry_ped.get().strip() or "10"])
        
        eng_custom = self.entry_custom_engine.get().strip()
        eng_selected = self.engine_var.get()
        if eng_custom:
            cmd.extend(["--engine", eng_custom])
        else:
            if eng_selected == "models/deeplabv3_mobilenet.engine":
                cmd.append("--deeplabv3")
            elif eng_selected == "models/fcn_resnet50.engine":
                cmd.append("--resnet50")
            else:
                cmd.extend(["--engine", eng_selected])
        
        if self.bgra_var.get():
            cmd.extend(["--assume_bgra", "1"])
        else:
            cmd.extend(["--assume_bgra", "0"])

        if self.nopred_var.get():
            cmd.extend(["--no_pred", "1"])
            
        if self.mem_var.get():
            cmd.append("--memory")
        if self.display_var.get():
            cmd.append("--display")
            
        apex = self.apex_var.get()
        if apex != "off":
            cmd.extend(["--apex-mode", apex])

        # Behavior Flags
        if self.dataset_var.get():
            cmd.append("--run-dataset")
        elif self.sweep_var.get():
            cmd.append("--run-sweep")
        else:
            cmd.extend(["--sched", self.sched_var.get()])
            cmd.append("--run-profiling")
            
        if self.split_var.get():
            cmd.append("--run-split")
            
        return " ".join(cmd)

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

        self.console.delete("1.0", "end")
        self.log_msg(f"[Exec] Initializing sequence:\n{cmd}\n")
        
        threading.Thread(target=self._exec_thread, args=(cmd,), daemon=True).start()

    def _exec_thread(self, cmd):
        try:
            self.current_process = subprocess.Popen(
                ["bash", "-c", cmd],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )

            for line in self.current_process.stdout:
                self.console.after(0, self.log_msg, line.strip('\n'))

            self.current_process.wait()
            self.console.after(0, self.log_msg, f"\n[Done] Process exited with code {self.current_process.returncode}\n====================================")
        except Exception as e:
            self.console.after(0, self.log_msg, f"\n[Error] Failed to execute safely: {str(e)}")
        finally:
            self.current_process = None
            subprocess.run(["./tools/kill_server.sh"], capture_output=True)

if __name__ == "__main__":
    app = LauncherApp()
    app.mainloop()
