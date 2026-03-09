import os
import sys
import time
import customtkinter as ctk
import mss
import mss.tools

# Append tools directory to import gui_launcher without executing __main__ conditionally
sys.path.append('tools')
from gui_launcher import LauncherApp

def capture_tabs():
    os.environ["DISPLAY"] = ":0" # ensure we connect to X display 0 if running in terminal
    try:
        app = LauncherApp()
    except Exception as e:
        print(f"Failed to start GUI for screenshots: {e}")
        return

    # Let the main widget render completely
    app.update()
    time.sleep(1)

    tabs = [
        "Simulation & Engine",
        "Environment & Output",
        "StarPU & Analyzers",
        "Camera & Output"
    ]

    with mss.mss() as sct:
        for i, tab_name in enumerate(tabs):
            app.tabview.set(tab_name)
            app.update()
            time.sleep(0.5)
            
            x = app.winfo_rootx()
            y = app.winfo_rooty()
            w = app.winfo_width()
            h = app.winfo_height()
            
            if w < 100 or h < 100:
                print(f"Warning: Dimensions {w}x{h} might be too small.")
                w = 1200
                h = 800
                
            region = {'top': y, 'left': x, 'width': w, 'height': h}
            try:
                sct_img = sct.grab(region)
                out_path = f'result_example/tab{i+1}_{tab_name.replace(" & ", "_").replace(" ", "_").lower()}.png'
                mss.tools.to_png(sct_img.rgb, sct_img.size, output=out_path)
                print(f"Saved: {out_path}")
            except Exception as e:
                print(f"Failed to grab screen: {e}")

    app.destroy()

if __name__ == "__main__":
    capture_tabs()
