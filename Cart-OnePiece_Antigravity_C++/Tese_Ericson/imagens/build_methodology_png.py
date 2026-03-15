import matplotlib.pyplot as plt
import matplotlib.patches as patches
import math

# Use equal aspect ratio based dimensions
fig_w, fig_h = 16.8, 9.6  # 420/25, 240/25 -> ratio 1.75
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(0, 420)
ax.set_ylim(0, 240)
ax.axis('off')

# Configuration
colors = [
    {
        'border': '#1F79A9', 'fill': '#F1F7FB', 'stage': 'Stage 1:', 'title': 'Systematic\nMapping Study',
        'bullets': [
            '- Collect and\n  analyze existing\n  literature', 
            '- Identify key\n  research trends\n  and gaps', 
            '- Classify prior\n  work'
        ]
    },
    {
        'border': '#4DA359', 'fill': '#F0F8F2', 'stage': 'Stage 2:', 'title': 'Experimental\nFeasibility',
        'bullets': [
            '- Develop initial\n  prototypes', 
            '- Conduct\n  proof-of-concept\n  experiments', 
            '- Assess\n  technical\n  viability'
        ]
    },
    {
        'border': '#E49A3E', 'fill': '#FEF7F0', 'stage': 'Stage 3:', 'title': 'Cart-OnePiece\nFramework',
        'bullets': [
            '- Propose new\n  theoretical\n  model', 
            '- Implement the\n  Cart-OnePiece\n  system', 
            '- Integrate\n  components\n  and protocols'
        ]
    },
    {
        'border': '#CF3F44', 'fill': '#FEF1F1', 'stage': 'Stage 4:', 'title': 'Empirical\nEvaluation',
        'bullets': [
            '- Execute\n  controlled\n  experiments', 
            '- Compare with\n  baseline\n  methods', 
            '- Analyze\n  quantitative\n  performance\n  data'
        ]
    }
]

box_w = 85
box_h = 200
gap = 20
start_x = 10
start_y = 20

# Drawing function for gears
def draw_gear(ax, cx, cy, radius, color):
    # inner circle
    ax.add_patch(patches.Circle((cx, cy), radius * 0.65, edgecolor=color, facecolor='none', lw=2, zorder=3))
    # outer gear teeth approximation
    for angle in range(0, 360, 45):
        rad = angle * math.pi / 180
        ax.plot([cx + radius*0.75*math.cos(rad), cx + radius*1.1*math.cos(rad)],
                [cy + radius*0.75*math.sin(rad), cy + radius*1.1*math.sin(rad)], color=color, lw=3, solid_capstyle='butt', zorder=3)
    ax.add_patch(patches.Circle((cx, cy), radius * 0.85, edgecolor=color, facecolor='none', lw=2, zorder=3))
    # very inner dot
    ax.add_patch(patches.Circle((cx, cy), radius * 0.2, edgecolor=color, facecolor=color, zorder=3))

for i, c in enumerate(colors):
    bx = start_x + i * (box_w + gap)
    by = start_y
    cx = bx + box_w / 2
    
    # Draw arrow before box
    if i > 0:
        arr_lx = start_x + (i-1)*(box_w+gap) + box_w
        arr_rx = bx
        arr_y = 120
        c_arrow = colors[i-1]['border']
        ax.plot([arr_lx, arr_rx - 1.5], [arr_y, arr_y], color=c_arrow, lw=3.0, zorder=1)
        # Open arrowhead
        ah_sz = 3.5
        ax.plot([arr_rx - 1 - ah_sz, arr_rx - 1], [arr_y + ah_sz, arr_y], color=c_arrow, lw=3.0, solid_capstyle='round', zorder=1)
        ax.plot([arr_rx - 1 - ah_sz, arr_rx - 1], [arr_y - ah_sz, arr_y], color=c_arrow, lw=3.0, solid_capstyle='round', zorder=1)

    # Box
    bbox = patches.FancyBboxPatch((bx, by), box_w, box_h, boxstyle="round,pad=2,rounding_size=6", 
                                  ec=c['border'], fc=c['fill'], lw=2.5, zorder=2)
    ax.add_patch(bbox)
    
    # Icon Base
    icon_cy = by + box_h - 18
    
    # Render Icons carefully
    if i == 0:
        # Magnifying glass
        rad = 8
        m_cx, m_cy = cx - 4, icon_cy + 4
        ax.add_patch(patches.Circle((m_cx, m_cy), rad, edgecolor=c['border'], facecolor='none', lw=2.8, zorder=3))
        # handle
        ax.plot([m_cx + 5, m_cx + 14], [m_cy - 5, m_cy - 14], color=c['border'], lw=4, solid_capstyle='round', zorder=3)
        # inner light reflection
        ax.add_patch(patches.Arc((m_cx, m_cy), rad*1.2, rad*1.2, theta1=110, theta2=160, edgecolor=c['border'], lw=1, zorder=3))
    
    elif i == 1:
        # Flask & Gears
        draw_gear(ax, cx - 6, icon_cy + 6, 5.5, c['border'])
        draw_gear(ax, cx - 12, icon_cy - 4, 3.5, c['border'])
        
        # Flask (Erlenmeyer)
        fx, fy = cx + 8, icon_cy - 6
        fw, fh = 14, 16
        # Outline
        flask_points = [[fx, fy+fh], [fx, fy+fh-4], [fx-fw/2, fy], [fx+fw/2, fy], [fx, fy+fh-4]]
        poly = patches.Polygon(flask_points, closed=True, edgecolor=c['border'], facecolor='none', lw=2.2, zorder=3, joinstyle='round')
        ax.add_patch(poly)
        # Liquid line & bubbles
        ax.plot([fx-fw*.35, fx+fw*.35], [fy+fh*.3, fy+fh*.3], color=c['border'], lw=1.5, zorder=3)
        ax.add_patch(patches.Circle((fx-2, fy+fh*.5), 0.8, edgecolor=c['border'], facecolor='none', lw=1, zorder=3))
        ax.add_patch(patches.Circle((fx+1, fy+fh*.7), 1.2, edgecolor=c['border'], facecolor='none', lw=1, zorder=3))
    
    elif i == 2:
        # Framework nodes
        sw, sh = 8, 5.5
        def box_node(nx, ny):
            ax.add_patch(patches.Rectangle((nx-sw/2, ny-sh/2), sw, sh, edgecolor=c['border'], facecolor='none', lw=2.2, zorder=3))
        
        box_node(cx, icon_cy + 8)
        box_node(cx - 10, icon_cy - 5)
        box_node(cx + 10, icon_cy - 5)
        box_node(cx, icon_cy - 5)
        # connectors
        ax.plot([cx, cx], [icon_cy+8-sh/2, icon_cy-5+sh/2], color=c['border'], lw=2, zorder=3)
        ax.plot([cx-10, cx+10], [icon_cy+1.5, icon_cy+1.5], color=c['border'], lw=2, zorder=3)
        ax.plot([cx-10, cx-10], [icon_cy+1.5, icon_cy-5+sh/2], color=c['border'], lw=2, zorder=3)
        ax.plot([cx+10, cx+10], [icon_cy+1.5, icon_cy-5+sh/2], color=c['border'], lw=2, zorder=3)
        # loop arrows approx
        ax.plot([cx-10+sw/2, cx-sw/2], [icon_cy-5, icon_cy-5], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx-sw/2 - 2, cx-sw/2], [icon_cy-5+1.5, icon_cy-5], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx-sw/2 - 2, cx-sw/2], [icon_cy-5-1.5, icon_cy-5], color=c['border'], lw=1.5, zorder=3)
        
        ax.plot([cx+sw/2, cx+10-sw/2], [icon_cy-5, icon_cy-5], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx+10-sw/2 - 2, cx+10-sw/2], [icon_cy-5+1.5, icon_cy-5], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx+10-sw/2 - 2, cx+10-sw/2], [icon_cy-5-1.5, icon_cy-5], color=c['border'], lw=1.5, zorder=3)
    
    elif i == 3:
        # Chart
        w = 3.5
        base_y = icon_cy - 7
        ax.add_patch(patches.Rectangle((cx-10, base_y), w, 6, edgecolor=c['border'], facecolor='none', lw=2.2, zorder=3))
        ax.add_patch(patches.Rectangle((cx-4, base_y), w, 11, edgecolor=c['border'], facecolor='none', lw=2.2, zorder=3))
        ax.add_patch(patches.Rectangle((cx+2, base_y), w, 16, edgecolor=c['border'], facecolor='none', lw=2.2, zorder=3))
        # axis line
        ax.plot([cx-12, cx+8], [base_y, base_y], color=c['border'], lw=2.2, zorder=3)
        # trend line
        ax.plot([cx-8.25, cx-2.25, cx+3.75], [base_y+4, base_y+8, base_y+14], color=c['border'], lw=2.5, zorder=3)
        ax.add_patch(patches.Circle((cx+3.75, base_y+14), 1.2, color=c['border'], zorder=3))
        # Checkmark circle
        ch_cx, ch_cy = cx+7, base_y+3
        ax.add_patch(patches.Circle((ch_cx, ch_cy), 4, edgecolor=c['border'], facecolor=c['fill'], lw=2, zorder=4))
        # Checkmark lines
        ax.plot([ch_cx-1.5, ch_cx-0.2, ch_cx+2.2], [ch_cy-0.5, ch_cy-1.5, ch_cy+1.5], color=c['border'], lw=2, solid_capstyle='round', zorder=5)

    # Texts
    text_x = bx + 6.5
    title_y = by + box_h - 40
    
    # Stage X: (Bold)
    ax.text(text_x, title_y, c['stage'], color=c['border'], fontfamily='sans-serif', 
            fontweight='bold', fontsize=21, ha='left', va='top', zorder=4)
    # Title Body (Bold)
    ax.text(text_x, title_y - 13, c['title'], color=c['border'], fontfamily='sans-serif', 
            fontweight='bold', fontsize=21, ha='left', va='top', zorder=4, linespacing=1.2)
    
    # Bullets
    bullets_start_y = title_y - 52
    cur_y = bullets_start_y
    for btext in c['bullets']:
        # We split to get accurate line height spacing for exact wrap replication
        lines = btext.split('\n')
        # Render first line with the dash
        ax.text(text_x, cur_y, lines[0], color='#222222', fontfamily='sans-serif', 
                fontsize=15, ha='left', va='top', zorder=4)
        cur_y -= 9.5
        # Render subsequent lines aligned past the dash
        for l in lines[1:]:
            ax.text(text_x, cur_y, l, color='#222222', fontfamily='sans-serif', 
                    fontsize=15, ha='left', va='top', zorder=4)
            cur_y -= 9.5
        
        cur_y -= 4 # gap between bullets

plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/methodology_stages_perfect.png', dpi=300, bbox_inches='tight', pad_inches=0.01)
print("Done saving ultra-high fidelity PNG.")
