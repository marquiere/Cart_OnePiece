import matplotlib.pyplot as plt
import matplotlib.patches as patches

fig_w, fig_h = 24, 12
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(-5, 275)
ax.set_ylim(-10, 120)
ax.axis('off')

# Title
ax.text(135, 115, "CArt-OnePiece Proposed Pipeline Architecture", fontsize=22, fontweight='bold', ha='center', va='center', fontfamily='sans-serif', color='#1A365D')

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
bw, bh = 34, 24
space_x = 10
row1_y = 75
row2_y = 25

# -----------------
# STAGE 1: Simulation Scenario
# -----------------
x1 = 0
draw_box(ax, x1, row1_y, bw, bh, "1. Simulation Scenario", 
         "• CARLA scenario execution\n• Ego vehicle & actor config\n• Dynamic environment setup", 
         c_carla['ec'], c_carla['fc'], c_carla['tfc'])

# -----------------
# STAGE 2: Sensor Config
# -----------------
x2 = x1 + bw + space_x
draw_box(ax, x2, row1_y, bw, bh, "2. Sensor Config & Selection", 
         "• Instantiate selected ADV sensors\n• Enable streams (single/multi)\n• Define acquisition parameters", 
         c_cpp['ec'], c_cpp['fc'], c_cpp['tfc'])
draw_arrow(ax, x1+bw, row1_y+bh/2, x2, row1_y+bh/2)

# -----------------
# STAGE 3: Data Acquisition Layer
# -----------------
x3 = x2 + bw + space_x
draw_box(ax, x3, row1_y, bw, bh, "3. Data Acquisition Layer", 
         "• Collect synchronized outputs\n• Buffer & organize incoming data\n• Expose data to processing modules", 
         c_cpp['ec'], c_cpp['fc'], c_cpp['tfc'])
draw_arrow(ax, x2+bw, row1_y+bh/2, x3, row1_y+bh/2)

# -----------------
# STAGE 4: Task Generation Layer
# -----------------
x4 = x3 + bw + space_x
draw_box(ax, x4, row1_y, bw, bh, "4. Task Generation Layer", 
         "• Stream -> Processing Tasks\n• Perception/Localization/Fusion\n• Define task dependencies", 
         c_cpp['ec'], c_cpp['fc'], c_cpp['tfc'])
draw_arrow(ax, x3+bw, row1_y+bh/2, x4, row1_y+bh/2)


# ==========================================================
# LARGE STARPU CONTAINER for Stages 5 and 6
# ==========================================================
sp_con_x = x4 + bw + space_x - 4
sp_con_w = (bw * 2) + space_x + 8
sp_con_h = bh * 3.8
sp_con_y = row2_y - 8
draw_large_container(ax, sp_con_x, sp_con_y, sp_con_w, sp_con_h, "StarPU Heterogeneous Runtime Environment", bg_starpu_ec, bg_starpu_fc, '#553C9A')

# -----------------
# STAGE 5: StarPU Scheduling
# -----------------
x5 = sp_con_x + 4
draw_box(ax, x5, row1_y, bw, bh, "5. Scheduling & Execution", 
         "• Submit generated tasks\n• Map tasks to CPU/GPU\n• Evaluate scheduling strategies", 
         c_starpu['ec'], 'white', c_starpu['tfc'])
draw_arrow(ax, x4+bw, row1_y+bh/2, x5, row1_y+bh/2)
ax.text(x4+bw+space_x/2, row1_y+bh/2+1, "Submit\nTask Graph", ha='center', va='bottom', fontsize=9, color='#4A5568')


# -----------------
# STAGE 6: Processing Modules
# -----------------
x6 = x5 + bw + space_x
draw_box(ax, x6, row1_y, bw, bh, "6. Processing & Analysis", 
         "• Execute specified kernels\n• AI / Classical / Control\n• Aggregate outputs", 
         c_starpu['ec'], 'white', c_starpu['tfc'])
draw_arrow(ax, x5+bw, row1_y+bh/2, x6, row1_y+bh/2)

# -----------------
# STAGE 7: Outputs
# -----------------
x7 = x5 + bw/2
y7 = row2_y
ew = bw * 1.8
draw_box(ax, x7, y7, ew, bh, "7. Outputs & Evaluation", 
         "• Predictions, control, traces, logs\n• Workload & latency analysis\n• Dataset / benchmarking artifacts", 
         c_eval['ec'], c_eval['fc'], c_eval['tfc'])

draw_arrow(ax, x6+bw/2, row1_y, x7 + ew/2, y7+bh, style="-|>", lw=2)

# Bottom Source
ax.text(135, -8, "Source: Elaborated by the Author.", fontsize=11, fontweight='bold', ha='center', va='center', fontfamily='serif')

plt.subplots_adjust(left=0.01, right=0.99, top=0.98, bottom=0.02)
filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/pipeline_architecture_v4.png'
plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0.05)
print(f"Done saving {filename}")
