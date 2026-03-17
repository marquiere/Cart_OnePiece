import matplotlib.pyplot as plt
import matplotlib.patches as patches
import math

fig_w, fig_h = 14, 12
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(0, 140)
ax.set_ylim(-5, 120)
ax.axis('off')

# Title 
ax.text(70, 115, "Figure 13 – Steps of our pipeline (CArt-OnePiece)", fontsize=16, fontweight='bold', ha='center', va='center', fontfamily='serif')

# Top Text
ax.text(70, 108, "Simulation Setup & Configuration", fontsize=14, ha='center', va='center', fontfamily='sans-serif')

# Top Arrow
ax.arrow(70, 105, 0, -4, head_width=1.5, head_length=1.5, fc='black', ec='black', lw=1.5)

# Dashed bounding box
dashed_box = patches.Rectangle((5, 12), 130, 90, fill=False, edgecolor='gray', linestyle='--', lw=1.0)
ax.add_patch(dashed_box)

def draw_cylinder(ax, x, y, w, h, text, ec, fc, lw=2.0, fontsize=12, text_color='black'):
    # A custom cylinder-like shape
    path_data = [
        (plt.matplotlib.path.Path.MOVETO, (x, y)),
        (plt.matplotlib.path.Path.LINETO, (x, y+h)),
        (plt.matplotlib.path.Path.CURVE3, (x+w/2, y+h+4)),
        (plt.matplotlib.path.Path.LINETO, (x+w, y+h)),
        (plt.matplotlib.path.Path.LINETO, (x+w, y)),
        (plt.matplotlib.path.Path.CURVE3, (x+w/2, y-4)),
        (plt.matplotlib.path.Path.LINETO, (x, y)),
        (plt.matplotlib.path.Path.CLOSEPOLY, (x, y))
    ]
    codes, verts = zip(*path_data)
    path = plt.matplotlib.path.Path(verts, codes)
    patch = patches.PathPatch(path, facecolor=fc, edgecolor=ec, lw=lw, zorder=2)
    ax.add_patch(patch)
    
    # Draw top arc to make it look 3D cylinder
    arc_data = [
        (plt.matplotlib.path.Path.MOVETO, (x, y+h)),
        (plt.matplotlib.path.Path.CURVE3, (x+w/2, y+h-4)),
        (plt.matplotlib.path.Path.LINETO, (x+w, y+h))
    ]
    acodes, averts = zip(*arc_data)
    apath = plt.matplotlib.path.Path(averts, acodes)
    apatch = patches.PathPatch(apath, facecolor='none', edgecolor=ec, lw=lw, zorder=3)
    ax.add_patch(apatch)

    ax.text(x + w/2, y + h/2, text, ha='center', va='center', fontsize=fontsize, color=text_color, fontfamily='sans-serif', fontweight='bold', zorder=4, linespacing=1.4)

def draw_small_box(ax, x, y, w, h, text, ec, fc, text_color='black'):
    box = patches.FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.2,rounding_size=1.5", ec=ec, fc=fc, lw=1.5, zorder=2)
    ax.add_patch(box)
    ax.text(x + w/2, y + h/2, text, ha='center', va='center', fontsize=11, color=text_color, fontfamily='sans-serif', zorder=3, linespacing=1.3)

def draw_arrow(ax, x1, y1, x2, y2, color, style="->"):
    ax.annotate("", xy=(x2, y2), xytext=(x1, y1), arrowprops=dict(arrowstyle=style, color=color, lw=1.5, shrinkA=0, shrinkB=0))


# Colors
c_center_ec = '#155B7A'
c_center_fc = '#F5FAFE'

c_left = {'ec': '#3C8E47', 'fc': '#F0F8F1'} # Light green like in Felipe's left boxes
c_right = {'ec': '#333333', 'fc': '#FFFFFF'}

c_w = 40
c_h = 10
c_x = 50

# Y positions
y1 = 80
y2 = 60
y3 = 40
y4 = 20

# Centers
draw_cylinder(ax, c_x, y1, c_w, c_h, "1 - Data Ingestion &\nSynchronization", c_center_ec, c_center_fc, fontsize=13, text_color='black')
draw_cylinder(ax, c_x, y2, c_w, c_h, "2 - Task-based\nPre-processing", c_center_ec, c_center_fc, fontsize=13, text_color='black')
draw_cylinder(ax, c_x, y3, c_w, c_h, "3 - Heterogeneous\nInference", c_center_ec, c_center_fc, fontsize=13, text_color='black')
draw_cylinder(ax, c_x, y4, c_w, c_h, "4 - Post-processing\n& Evaluation", c_center_ec, c_center_fc, fontsize=13, text_color='black')

# Center arrows
draw_arrow(ax, 70, y1-3, 70, y2+c_h+2, 'black', style="-|>")
draw_arrow(ax, 70, y2-3, 70, y3+c_h+2, 'black', style="-|>")
draw_arrow(ax, 70, y3-3, 70, y4+c_h+2, 'black', style="-|>")

# Left side
l_w = 26
l_h = 6
l_x = 12
draw_small_box(ax, l_x, y1+2, l_w, l_h, "CARLA Simulator &\nC++ Client", c_left['ec'], c_left['fc'])
draw_arrow(ax, c_x, y1+6, l_x+l_w, y1+6, c_left['ec'], "->")
draw_arrow(ax, l_x+l_w, y1+4, c_x, y1+4, c_left['ec'], "->")

draw_small_box(ax, l_x, y2+2, l_w, l_h, "StarPU CPU Workers", c_left['ec'], c_left['fc'])
draw_arrow(ax, l_x+l_w, y2+5, c_x, y2+5, c_left['ec'], "->")

draw_small_box(ax, l_x, y3+2, l_w, l_h, "StarPU GPU Workers", c_left['ec'], c_left['fc'])
draw_arrow(ax, l_x+l_w, y3+5, c_x, y3+5, c_left['ec'], "->")

draw_small_box(ax, l_x, y4+6, l_w, l_h, "Evaluation Thread", c_left['ec'], c_left['fc'])
draw_small_box(ax, l_x, y4-2, l_w, l_h, "Result Viewer GUI", c_left['ec'], c_left['fc'])
draw_arrow(ax, c_x, y4+9, l_x+l_w, y4+9, c_left['ec'], "->")
draw_arrow(ax, c_x, y4+1, l_x+l_w, y4+1, c_left['ec'], "->")


# Right side
r_w = 36
r_h = 5
r_x = 98

draw_small_box(ax, r_x, y1+6, r_w, r_h, "RGB & SemSeg Frames", c_right['ec'], c_right['fc'])
draw_small_box(ax, r_x, y1-1, r_w, r_h, "FrameSync Buffering", c_right['ec'], c_right['fc'])
draw_arrow(ax, c_x+c_w, y1+8.5, r_x, y1+8.5, 'black', "->")
draw_arrow(ax, c_x+c_w, y1+1.5, r_x, y1+1.5, 'black', "->")

draw_small_box(ax, r_x, y2+2.5, r_w, r_h, "BGRA $\\rightarrow$ NCHW & Normalization", c_right['ec'], c_right['fc'])
draw_arrow(ax, c_x+c_w, y2+5, r_x, y2+5, 'black', "->")

draw_small_box(ax, r_x, y3+2.5, r_w, r_h, "TensorRT Engine Execution", c_right['ec'], c_right['fc'])
draw_arrow(ax, c_x+c_w, y3+5, r_x, y3+5, 'black', "->")

draw_small_box(ax, r_x, y4+9, r_w, r_h, "Direct Labels / Logits Decode", c_right['ec'], c_right['fc'])
draw_small_box(ax, r_x, y4+2, r_w, r_h, "mIoU & Pixel Accuracy Mapping", c_right['ec'], c_right['fc'])
draw_small_box(ax, r_x, y4-5, r_w, r_h, "FxT Tracing & CSV Logs", c_right['ec'], c_right['fc'])
draw_arrow(ax, c_x+c_w, y4+11.5, r_x, y4+11.5, 'black', "->")
draw_arrow(ax, c_x+c_w, y4+4.5, r_x, y4+4.5, 'black', "->")
draw_arrow(ax, c_x+c_w, y4-2.5, r_x, y4-2.5, 'black', "->")


# Bottom Text
ax.arrow(70, 12, 0, -5, head_width=1.5, head_length=1.5, fc='black', ec='black', lw=1.5)
ax.text(70, 3, "Performance Artifacts & System Logs", fontsize=14, ha='center', va='center', fontfamily='sans-serif')

# Add 'Source: Elaborated by the Author.' below image
ax.text(70, -2, "Source: Elaborated by the Author.", fontsize=11, fontweight='bold', ha='center', va='center', fontfamily='serif')

plt.subplots_adjust(left=0, right=1, top=1, bottom=0)
filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/pipeline_architecture_v1.png'
plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0.01)
print(f"Done saving {filename}")
