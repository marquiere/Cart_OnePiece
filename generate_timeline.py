import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import textwrap

# Data for the stages based on the user's specific request
stages = [
    {
        "title": "Stage 1:\nSystematic\nMapping Study",
        "items": [
            "- Identify ADV trends",
            "- Analyze key technologies",
            "- Establish research basis"
        ],
        "color": "#f0f8ff", "edge": "#4682b4", "title_col": "#206090", "icon": "magnifying glass"
    },
    {
        "title": "Stage 2:\nIntegration\nFeasibility",
        "items": [
            "- Evaluate CARLA–StarPU\n  integration",
            "- Validate experimental\n  viability",
            "- Define the methodological\n  basis"
        ],
        "color": "#f0fff0", "edge": "#2e8b57", "title_col": "#155e30", "icon": "gears & flask"
    },
    {
        "title": "Stage 3:\nFramework\nConsolidate",
        "items": [
            "- Consolidate Cart-OnePiece",
            "- Support workload\n  investigations",
            "- Enable systematic\n  experimentation"
        ],
        "color": "#fffaf0", "edge": "#cd853f", "title_col": "#aa6010", "icon": "blocks"
    },
    {
        "title": "Stage 4:\nEmpirical\nEvaluation",
        "items": [
            "- Execute controlled\n  experiments",
            "- Collect runtime \n  profiling data",
            "- Compare scheduling\n  strategies"
        ],
        "color": "#fff0f5", "edge": "#cd5c5c", "title_col": "#a02020", "icon": "chart"
    }
]

fig, ax = plt.subplots(figsize=(14, 8))
# Set background color similar to the image
fig.patch.set_facecolor('#f4f9fd')
ax.set_facecolor('#f4f9fd')
ax.axis('off')

# Dimensions
box_w = 2.4
box_h = 4.2
spacing = 0.5
start_x = 1.0
y = 2.0

# Add some faint background grid lines to mimic the picture
for grid_x in range(0, 15, 2):
    ax.axvline(x=grid_x, color='#e9ecef', linewidth=1, zorder=0)
for grid_y in range(1, 8, 2):
    ax.axhline(y=grid_y, color='#e9ecef', linewidth=1, zorder=0)

for i, stage in enumerate(stages):
    x = start_x + i * (box_w + spacing)
    
    # Shadow
    rect_shadow = patches.FancyBboxPatch(
        (x + 0.04, y - 0.04), box_w, box_h,
        boxstyle="round,pad=0.1", facecolor='#e0e0e0', edgecolor='none', zorder=1
    )
    ax.add_patch(rect_shadow)
    
    # Box Background
    rect = patches.FancyBboxPatch(
        (x, y), box_w, box_h,
        boxstyle="round,pad=0.1",
        facecolor='white', edgecolor=stage["edge"],
        linewidth=1.5, zorder=2
    )
    ax.add_patch(rect)
    
    # Title Box (colored upper half)
    title_h = 1.2
    # We create a clipping path with the rounded box, then draw a rect inside
    title_rect = patches.FancyBboxPatch(
        (x, y+box_h-title_h), box_w, title_h,
        boxstyle="round,pad=0.1",
        facecolor=stage["color"], edgecolor='none',
        zorder=3
    )
    # Actually just overlay a colored box on top of the rounded box and clip it?
    # Simple workaround: Just draw a standard rectangle without rounded bottom
    import matplotlib.path as mpath
    
    # We'll just put a colored rectangle at the top of the box. The boundary is drawn over it anyway.
    ax.add_patch(rect)
    
    # Text Placement
    # Title
    ax.text(x + 0.2, y + box_h - 0.4, stage["title"], 
            fontsize=15, fontweight='bold', color=stage["title_col"], zorder=4,
            verticalalignment='top', linespacing=1.2)
    
    # Separator Line
    ax.plot([x+0.1, x+box_w-0.1], [y+box_h-title_h-0.2, y+box_h-title_h-0.2], color=stage["edge"], alpha=0.3, zorder=4)

    # Put a fake icon circle at the top
    # ax.add_patch(patches.Circle((x + box_w/2, y + box_h - 0.2), 0.2, edgecolor=stage["edge"], facecolor='none', lw=1.5, zorder=4))
    
    # Items
    item_start_y = y + box_h - title_h - 0.6
    for j, item in enumerate(stage["items"]):
        ax.text(x + 0.15, item_start_y, item, 
                fontsize=13, color='#333', zorder=4,
                verticalalignment='top', linespacing=1.6)
        
        # Calculate approximate height of this text to push the next item down
        item_start_y -= (len(item.split('\n')) * 0.35 + 0.1)
                
    # Arrow Rightwards
    if i < len(stages) - 1:
        arr_x = x + box_w + 0.1
        arr_y = y + box_h/2
        
        ax.annotate('', xy=(arr_x + spacing - 0.2, arr_y),
                    xytext=(arr_x, arr_y),
                    arrowprops=dict(facecolor='none', edgecolor=stages[i+1]["edge"], 
                                    width=1.5, headwidth=10, headlength=10, zorder=1, linewidth=2))

plt.xlim(0, 14)
plt.ylim(0, 8)
plt.tight_layout()
plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/texto/timeline_phase.png', dpi=300, bbox_inches='tight')
plt.savefig('/home/eric/.gemini/antigravity/brain/0af1de37-af29-44ac-8da9-c69d5faaf668/timeline_phase_py.png', dpi=300, bbox_inches='tight')

print("Saved figure using python.")
