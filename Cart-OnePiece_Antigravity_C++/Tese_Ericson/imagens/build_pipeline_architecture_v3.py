import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.patheffects as pe

fig_w, fig_h = 20, 10
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(-5, 235)
ax.set_ylim(-10, 100)
ax.axis('off')

# Title
ax.text(115, 95, "CArt-OnePiece Execution Pipeline Architecture", fontsize=20, fontweight='bold', ha='center', va='center', fontfamily='sans-serif', color='#1A365D')

def draw_box(ax, x, y, w, h, title, body_text, ec, fc, title_fc, zorder=2):
    # Main Box
    box = patches.FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.2,rounding_size=2", ec=ec, fc=fc, lw=2, zorder=zorder)
    ax.add_patch(box)
    
    # Title Header Box (Top portion)
    header_h = h * 0.25
    header = patches.FancyBboxPatch((x, y + h - header_h), w, header_h, boxstyle="round,pad=0.2,rounding_size=2", ec=ec, fc=title_fc, lw=2, zorder=zorder+1)
    
    # Square off the bottom of the header
    ax.add_patch(header)
    rect = patches.Rectangle((x, y + h - header_h), w, header_h/2, ec='none', fc=title_fc, zorder=zorder+2)
    ax.add_patch(rect)
    ax.plot([x, x+w], [y + h - header_h, y + h - header_h], color=ec, lw=2, zorder=zorder+3)

    # Title Text
    ax.text(x + w/2, y + h - header_h/2, title, ha='center', va='center', fontsize=11.5, fontweight='bold', color='white', fontfamily='sans-serif', zorder=zorder+4)
    
    # Body Text (Bullets)
    ax.text(x + 2, y + h - header_h - 4, body_text, ha='left', va='top', fontsize=11, color='#2D3748', fontfamily='sans-serif', zorder=zorder+4, linespacing=1.6)

def draw_large_container(ax, x, y, w, h, title, ec, fc, text_color):
    box = patches.FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.5,rounding_size=3", ec=ec, fc=fc, lw=2.5, linestyle='--', zorder=0)
    ax.add_patch(box)
    
    # Label pill
    pw, ph = len(title)*1.5, 6
    pill = patches.FancyBboxPatch((x + w/2 - pw/2, y + h - ph/2), pw, ph, boxstyle="round,pad=0.2,rounding_size=2.5", ec=ec, fc=fc, lw=2.5, zorder=1)
    ax.add_patch(pill)
    ax.text(x + w/2, y + h, title, ha='center', va='center', fontsize=14, fontweight='bold', color=text_color, fontfamily='sans-serif', zorder=2)

def draw_arrow(ax, x1, y1, x2, y2, color='#4A5568', lw=2.5, style="-|>", ls="-"):
    ax.annotate("", xy=(x2, y2), xytext=(x1, y1), arrowprops=dict(arrowstyle=style, color=color, lw=lw, ls=ls, shrinkA=0, shrinkB=0, joinstyle='round'))

# Colors
c_carla = {'ec': '#2B6CB0', 'fc': '#EBF8FF', 'tfc': '#3182CE'}
c_cpp = {'ec': '#C05621', 'fc': '#FFFAF0', 'tfc': '#DD6B20'}
c_starpu = {'ec': '#6B46C1', 'fc': '#FAF5FF', 'tfc': '#805AD5'}
c_eval = {'ec': '#276749', 'fc': '#F0FFF4', 'tfc': '#38A169'}
c_tracing = {'ec': '#718096', 'fc': '#F7FAFC', 'tfc': '#A0AEC0'}

bg_starpu_ec = '#9F7AEA'
bg_starpu_fc = '#FAF5FF'

# Sizing and Layout
bw, bh = 32, 22
space_x = 12
row1_y = 65
row2_y = 20

# -----------------
# STAGE 1: CARLA
# -----------------
x1 = 0
draw_box(ax, x1, row1_y, bw, bh, "1. Simulation\nEnvironment", 
         "• CARLA Server (UE4/UE5)\n• Auto-pilot TM\n• Generic Sensors Spawn (RGB, LiDAR...)\n• Multi-agent state", 
         c_carla['ec'], c_carla['fc'], c_carla['tfc'])

# -----------------
# STAGE 2: C++ Client
# -----------------
x2 = x1 + bw + space_x
draw_box(ax, x2, row1_y, bw, bh, "2. Universal Data\nIngestion", 
         "• Dynamic Sensor Listeners\n• Asynchronous Buffer Allocation\n• Configurable Sync Threads\n• Dispatch to Local Engine", 
         c_cpp['ec'], c_cpp['fc'], c_cpp['tfc'])
draw_arrow(ax, x1+bw, row1_y+bh/2, x2, row1_y+bh/2)
ax.text(x1+bw+space_x/2, row1_y+bh/2+1, "Heterogeneous\nRaw Data", ha='center', va='bottom', fontsize=9, color='#4A5568')

# ==========================================================
# LARGE STARPU CONTAINER for Stages 3, 4, 5
# ==========================================================
sp_con_x = x2 + bw + space_x - 4
sp_con_w = (bw * 3) + (space_x * 2) + 8
sp_con_h = bh * 3.6
sp_con_y = row2_y - 8
draw_large_container(ax, sp_con_x, sp_con_y, sp_con_w, sp_con_h, "StarPU Heterogeneous Runtime Environment", bg_starpu_ec, bg_starpu_fc, '#553C9A')

# Arrow entering StarPU
draw_arrow(ax, x2+bw, row1_y+bh/2, sp_con_x+6, row1_y+bh/2)
ax.text(x2+bw+space_x/2, row1_y+bh/2+1, "Submit\nTask Graph", ha='center', va='bottom', fontsize=9, color='#4A5568', fontweight='bold')


# -----------------
# STAGE 3: Pre-processing
# -----------------
x3 = sp_con_x + 4
draw_box(ax, x3, row1_y, bw, bh, "3. Generic Task:\nData Structuring", 
         "• CPU/GPU Workers\n• Filtering & Resizing\n• Point Cloud Voxelization\n• Sensor Calibration\n• Buffer Format Conversion", 
         c_starpu['ec'], 'white', c_starpu['tfc'])

# -----------------
# STAGE 4: Inference
# -----------------
x4 = x3 + bw + space_x
draw_box(ax, x4, row1_y, bw, bh, "4. Generic Task:\nCompute / Fusion", 
         "• Heterogeneous Execution\n• Neural Network Inference\n• Sensor Fusion Algorithms\n• State Estimation / Tracking\n• Shared Memory Management", 
         c_starpu['ec'], 'white', c_starpu['tfc'])
draw_arrow(ax, x3+bw, row1_y+bh/2, x4, row1_y+bh/2)
ax.text(x3+bw+space_x/2, row1_y+bh/2+1, "Data dependency\nDAGs", ha='center', va='bottom', fontsize=9, color='#4A5568', fontweight='bold')


# -----------------
# STAGE 5: Post-processing
# -----------------
x5 = x4 + bw + space_x
draw_box(ax, x5, row1_y, bw, bh, "5. Generic Task:\nActuation / Output", 
         "• CPU Worker Execution\n• Data Decoding & Packing\n• Extracted Metadata\n• Path Planning Commands\n• Send back to Simulation", 
         c_starpu['ec'], 'white', c_starpu['tfc'])
draw_arrow(ax, x4+bw, row1_y+bh/2, x5, row1_y+bh/2)
ax.text(x4+bw+space_x/2, row1_y+bh/2+1, "Results\nDependence", ha='center', va='bottom', fontsize=9, color='#4A5568', fontweight='bold')


# -----------------
# STAGE 6: Evaluation
# -----------------
x6 = x4 + bw/2
y6 = row2_y
# Make eval box wider to fit text safely
ew = bw * 1.8
draw_box(ax, x6, y6, ew, bh, "6. Offline Evaluation & Storage", 
         "• Asynchronous Generic Threads (Post-callback)\n• Format Agnostic Metric Comparison\n• Workload serialization & Latency measurement", 
         c_eval['ec'], c_eval['fc'], c_eval['tfc'])

# Arrow from Stage 5 to Stage 6
draw_arrow(ax, x5+bw/2, row1_y, x6 + ew/2, y6+bh, style="-|>", lw=2)


# -----------------
# STAGE 7: FxT Profiling 
# -----------------
x7 = x3
y7 = row2_y
tw = bw * 1.4
draw_box(ax, x7, y7, tw, bh, "7. FxT & Pipeline Tracing", 
         "• Paje Trace Generation (`starpu_fxt`)\n• DAG extraction (data dependencies)\n• CSV Latency Logs (`g_pipeline_csv`)", 
         c_tracing['ec'], c_tracing['fc'], c_tracing['tfc'])

# Indicate profiling extracts from StarPU runtime directly
draw_arrow(ax, sp_con_x+2, y7+bh/2, x7, y7+bh/2, style="->", ls="dashed")

# Bottom Source
ax.text(115, -8, "Source: Elaborated by the Author.", fontsize=11, fontweight='bold', ha='center', va='center', fontfamily='serif')

plt.subplots_adjust(left=0.01, right=0.99, top=0.98, bottom=0.02)
filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/pipeline_architecture_v3.png'
plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0.05)
print(f"Done saving {filename}")

