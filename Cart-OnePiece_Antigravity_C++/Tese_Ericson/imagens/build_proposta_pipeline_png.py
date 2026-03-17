import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches

# Canvas Setup
fig_w, fig_h = 16, 9
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(0, 160)
ax.set_ylim(0, 90)
ax.axis('off')

# Colors mapped to components
c_carla = '#2E86C1'
c_starpu_bg = '#F8F9F9'
c_starpu_border = '#BDC3C7'
c_cpu = '#27AE60'
c_gpu = '#E67E22'
c_metric = '#8E44AD'
c_apex = '#34495E'

def draw_box(ax, x, y, w, h, text, title_color, bg_color, border_color, title_size=14, text_size=11):
    # Rounded box
    box = patches.FancyBboxPatch(
        (x, y), w, h, 
        boxstyle="round,pad=2,rounding_size=3", 
        ec=border_color, fc=bg_color, lw=2.5, zorder=2
    )
    ax.add_patch(box)
    
    # Text split into bold title and normal text
    lines = text.split('\n')
    # Title
    ax.text(x + w/2, y + h - 5, lines[0], 
            color=title_color, fontsize=title_size, fontweight='bold', 
            ha='center', va='top', zorder=3, fontfamily='sans-serif')
    # Subtitle or description
    if len(lines) > 1:
        # Check if the second line is a sub-title like "(Server)"
        desc_start_idx = 1
        if lines[1].startswith('('):
            ax.text(x + w/2, y + h - 11, lines[1], 
                    color=title_color, fontsize=title_size-2, fontweight='bold', 
                    ha='center', va='top', zorder=3, fontfamily='sans-serif')
            desc_start_idx = 2
            
        # Description lines
        if len(lines) > desc_start_idx:
            desc_text = '\n'.join(lines[desc_start_idx:])
            # Calculate top starting Y based on if there was a subtitle
            desc_y = y + h - 22 if desc_start_idx > 1 else y + h - 15
            ax.text(x + w/2, desc_y, desc_text, 
                    color='#2C3E50', fontsize=text_size, 
                    ha='center', va='top', zorder=3, fontfamily='sans-serif')

def draw_arrow(ax, x1, y1, x2, y2, color, lw=2.5):
    ax.annotate('', xy=(x2, y2), xytext=(x1, y1), 
                arrowprops=dict(facecolor=color, edgecolor=color, width=lw, headwidth=lw*3.5, shrink=0, headlength=lw*4), 
                zorder=1)

# CARLA Block
draw_box(ax, 5, 45, 28, 25, 
         "CARLA Simulator\n(Server)\n\nEnvironment\nSimulation &\nVirtual Cameras", 
         c_carla, '#EBF5FB', c_carla)

# StarPU Background Map
ax.add_patch(patches.FancyBboxPatch(
    (42, 10), 115, 75, 
    boxstyle="round,pad=2,rounding_size=4", 
    ec=c_starpu_border, fc=c_starpu_bg, lw=2, zorder=0, linestyle='--'
))
ax.text(45, 81, "StarPU Runtime Client", color='#7F8C8D', fontsize=18, fontweight='bold', ha='left', va='center', fontfamily='sans-serif')

# Preprocessing
draw_box(ax, 48, 45, 28, 25, 
         "Preprocessing Stage\n(CPU Workers)\n\nBGRA to NCHW\nNormalization", 
         c_cpu, '#EAFAF1', c_cpu)

# Inference
draw_box(ax, 88, 45, 28, 25, 
         "Inference Stage\n(GPU / TensorRT)\n\nResNet50 / DeepLabV3\nSemantic Masks", 
         c_gpu, '#FEF5E7', c_gpu)

# Metrics
draw_box(ax, 128, 45, 28, 25, 
         "Metric Calculation\n(Evaluator)\n\nComparison\nmIoU & Pixel Acc", 
         c_metric, '#F4ECF7', c_metric)

# Profiling
draw_box(ax, 58, 15, 88, 15, 
         "APEX Profiling & FxT Tracing\n\nRecords Latency, Memory Utilization & Structural Task Dependencies", 
         c_apex, '#EAECEE', c_apex, title_size=13, text_size=11)

# Main Flow Arrows
draw_arrow(ax, 33, 60, 48, 60, c_carla)
ax.text(40.5, 62, "Raw Visual\nData (BGRA)", color=c_carla, fontsize=9.5, fontweight='bold', ha='center', va='bottom', fontfamily='sans-serif')

draw_arrow(ax, 76, 57.5, 88, 57.5, c_cpu)
ax.text(82, 59.5, "Normalized\nTensors", color=c_cpu, fontsize=10, fontweight='bold', ha='center', va='bottom', fontfamily='sans-serif')

draw_arrow(ax, 116, 57.5, 128, 57.5, c_gpu)
ax.text(122, 59.5, "Semantic\nPredictions", color=c_gpu, fontsize=10, fontweight='bold', ha='center', va='bottom', fontfamily='sans-serif')

# Ground truth routing (Below the main pipeline, directly from CARLA to Metrics)
gt_y = 35
ax.plot([19, 19], [45, gt_y], color=c_carla, lw=2.5, zorder=1)
ax.plot([19, 142], [gt_y, gt_y], color=c_carla, lw=2.5, zorder=1)
draw_arrow(ax, 142, gt_y, 142, 45, c_carla)

# Ground truth label
ax.add_patch(patches.Rectangle((70, gt_y - 2.5), 65, 5, facecolor='white', edgecolor='none', zorder=2))
ax.text(102.5, gt_y, "Semantic Segmentation Ground Truth", color=c_carla, fontsize=11, fontweight='bold', ha='center', va='center', zorder=3, fontfamily='sans-serif')

# Save execution
filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/proposta_pipeline_draft.png'
plt.subplots_adjust(left=0, right=1, top=1, bottom=0)
plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0.1)
print(f"Done saving {filename}")
