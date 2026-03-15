import matplotlib.pyplot as plt
import matplotlib.patches as patches
import math

# Based on the exact new reference:
# 1. Boxes have two background sections: 
#    - Top section (Icon + Title) is slightly tinted with the stage color
#    - Bottom section (Bullets) is white/very faint grey
# 2. Border color
# 3. Fonts: Titles are bold and compact.
# 4. Spacing: Very tight horizontal gaps.
# 5. Arrows: Open barbed arrows between boxes.

# Canvas setup
fig_w, fig_h = 17.5, 9.8  
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(0, 420)
ax.set_ylim(0, 240)
ax.axis('off')

# Colors mapped exactly from the screenshot
colors = [
    {
        'border': '#4181AC', 'top_fill': '#EAF3FA', 'bot_fill': '#F4F9FD', 'stage': 'Stage 1:', 'title': 'Systematic\nMapping Study',
        'bullets': [
            '- Collect and\n  analyze existing\n  literature', 
            '- Identify key\n  research trends\n  and gaps', 
            '- Classify prior\n  work'
        ]
    },
    {
        'border': '#59A662', 'top_fill': '#EAF6EC', 'bot_fill': '#F5FDF6', 'stage': 'Stage 2:', 'title': 'Experimental\nFeasibility',
        'bullets': [
            '- Develop initial\n  prototypes', 
            '- Conduct\n  proof-of-concept\n  experiments', 
            '- Assess\n  technical\n  viability'
        ]
    },
    {
        'border': '#E49A4C', 'top_fill': '#FDEDDE', 'bot_fill': '#FEF7F0', 'stage': 'Stage 3:', 'title': 'Cart-OnePiece\nFramework',
        'bullets': [
            '- Propose new\n  theoretical\n  model', 
            '- Implement the\n  Cart-OnePiece\n  system', 
            '- Integrate\n  components\n  and protocols'
        ]
    },
    {
        'border': '#C74149', 'top_fill': '#FCE7E7', 'bot_fill': '#FEF2F2', 'stage': 'Stage 4:', 'title': 'Empirical\nEvaluation',
        'bullets': [
            '- Execute\n  controlled\n  experiments', 
            '- Compare with\n  baseline\n  methods', 
            '- Analyze\n  quantitative\n  performance\n  data'
        ]
    }
]

box_w = 84
box_h = 210
gap = 13
start_x = 18
start_y = 15

# Helper for gear
def draw_gear(ax, cx, cy, radius, color):
    ax.add_patch(patches.Circle((cx, cy), radius * 0.65, edgecolor=color, facecolor='none', lw=1.8, zorder=3))
    for angle in range(0, 360, 45):
        rad = angle * math.pi / 180
        ax.plot([cx + radius*0.75*math.cos(rad), cx + radius*1.1*math.cos(rad)],
                [cy + radius*0.75*math.sin(rad), cy + radius*1.1*math.sin(rad)], color=color, lw=2.5, solid_capstyle='butt', zorder=3)
    ax.add_patch(patches.Circle((cx, cy), radius * 0.85, edgecolor=color, facecolor='none', lw=1.8, zorder=3))
    ax.add_patch(patches.Circle((cx, cy), radius * 0.2, edgecolor=color, facecolor=color, zorder=3))

for i, c in enumerate(colors):
    bx = start_x + i * (box_w + gap)
    by = start_y
    cx = bx + box_w / 2
    
    # 1. DRAW ARROWS
    if i > 0:
        arr_lx = start_x + (i-1)*(box_w+gap) + box_w
        arr_rx = bx
        arr_y = by + box_h/2.0 + 8 # Arrow placed slightly above center
        c_arrow = colors[i-1]['border']
        ax.plot([arr_lx, arr_rx - 1], [arr_y, arr_y], color=c_arrow, lw=2.5, zorder=1)
        ah_sz = 3.5
        ax.plot([arr_rx - 1 - ah_sz, arr_rx - 1], [arr_y + ah_sz, arr_y], color=c_arrow, lw=2.5, solid_capstyle='round', zorder=1)
        ax.plot([arr_rx - 1 - ah_sz, arr_rx - 1], [arr_y - ah_sz, arr_y], color=c_arrow, lw=2.5, solid_capstyle='round', zorder=1)

    # 2. DRAW BOXES (Two-Tone approach)
    # The reference image has a two-tone box separated near the bullets
    split_y = by + box_h - 78
    
    # Bottom Fill (Clip to rounded box)
    clip_box = patches.FancyBboxPatch((bx, by), box_w, box_h, boxstyle="round,pad=2,rounding_size=5", lw=0, zorder=0)
    ax.add_patch(clip_box)
    
    # We use actual rectangles clipped to the rounded box to get the perfect two-tone fill
    bot_rect = patches.Rectangle((bx-4, by-4), box_w+8, split_y-by+4, facecolor=c['bot_fill'], zorder=1)
    top_rect = patches.Rectangle((bx-4, split_y), box_w+8, by+box_h-split_y+4, facecolor=c['top_fill'], zorder=1)
    bot_rect.set_clip_path(clip_box)
    top_rect.set_clip_path(clip_box)
    ax.add_patch(bot_rect)
    ax.add_patch(top_rect)
    
    # Main Border (Draw over the filled regions)
    outer_border = patches.FancyBboxPatch((bx, by), box_w, box_h, boxstyle="round,pad=2,rounding_size=5", 
                                        ec=c['border'], fc='none', lw=2.2, zorder=2)
    ax.add_patch(outer_border)
    
    # 3. ICONS
    icon_cy = by + box_h - 22
    
    if i == 0:
        rad = 6.5
        m_cx, m_cy = cx - 4, icon_cy + 4
        ax.add_patch(patches.Circle((m_cx, m_cy), rad, edgecolor=c['border'], facecolor='none', lw=2.2, zorder=3))
        ax.plot([m_cx + 4.5, m_cx + 12], [m_cy - 4.5, m_cy - 12], color=c['border'], lw=3.2, solid_capstyle='round', zorder=3)
        ax.add_patch(patches.Arc((m_cx, m_cy), rad*1.3, rad*1.3, theta1=110, theta2=160, edgecolor=c['border'], lw=1.2, zorder=3))
    elif i == 1:
        draw_gear(ax, cx - 6, icon_cy + 6, 4.8, c['border'])
        draw_gear(ax, cx - 11, icon_cy - 2, 3.0, c['border'])
        fx, fy = cx + 8, icon_cy - 5
        fw, fh = 12, 14
        flask_points = [[fx, fy+fh], [fx, fy+fh-3], [fx-fw/2, fy], [fx+fw/2, fy], [fx, fy+fh-3]]
        poly = patches.Polygon(flask_points, closed=True, edgecolor=c['border'], facecolor='none', lw=2.0, zorder=3, joinstyle='round')
        ax.add_patch(poly)
        ax.plot([fx-fw*.38, fx+fw*.38], [fy+fh*.28, fy+fh*.28], color=c['border'], lw=1.5, zorder=3)
        ax.add_patch(patches.Circle((fx-1.5, fy+fh*.5), 0.6, edgecolor=c['border'], facecolor='none', lw=0.8, zorder=3))
        ax.add_patch(patches.Circle((fx+1.2, fy+fh*.7), 0.9, edgecolor=c['border'], facecolor='none', lw=0.8, zorder=3))
    elif i == 2:
        sw, sh = 7, 5
        def box_node(nx, ny):
            ax.add_patch(patches.Rectangle((nx-sw/2, ny-sh/2), sw, sh, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        box_node(cx, icon_cy + 8)
        box_node(cx - 9, icon_cy - 4)
        box_node(cx + 9, icon_cy - 4)
        box_node(cx, icon_cy - 4)
        ax.plot([cx, cx], [icon_cy+8-sh/2, icon_cy-4+sh/2], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx-9, cx+9], [icon_cy+1.5, icon_cy+1.5], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx-9, cx-9], [icon_cy+1.5, icon_cy-4+sh/2], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx+9, cx+9], [icon_cy+1.5, icon_cy-4+sh/2], color=c['border'], lw=1.8, zorder=3)
        # loop arrows
        ax.plot([cx-9+sw/2, cx-sw/2], [icon_cy-4, icon_cy-4], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx-sw/2 - 1.5, cx-sw/2], [icon_cy-4+1.5, icon_cy-4], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx-sw/2 - 1.5, cx-sw/2], [icon_cy-4-1.5, icon_cy-4], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx+sw/2, cx+9-sw/2], [icon_cy-4, icon_cy-4], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx+9-sw/2 - 1.5, cx+9-sw/2], [icon_cy-4+1.5, icon_cy-4], color=c['border'], lw=1.5, zorder=3)
        ax.plot([cx+9-sw/2 - 1.5, cx+9-sw/2], [icon_cy-4-1.5, icon_cy-4], color=c['border'], lw=1.5, zorder=3)
    elif i == 3:
        w = 3.0
        base_y = icon_cy - 6
        ax.add_patch(patches.Rectangle((cx-9, base_y), w, 5, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.add_patch(patches.Rectangle((cx-4, base_y), w, 10, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.add_patch(patches.Rectangle((cx+1, base_y), w, 14, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.plot([cx-11, cx+7], [base_y, base_y], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx-7.5, cx-2.5, cx+2.5], [base_y+3, base_y+7, base_y+12], color=c['border'], lw=2.2, zorder=3)
        ax.add_patch(patches.Circle((cx+2.5, base_y+12), 1.0, color=c['border'], zorder=3))
        ch_cx, ch_cy = cx+7, base_y+2
        ax.add_patch(patches.Circle((ch_cx, ch_cy), 3.5, edgecolor=c['border'], facecolor=c['top_fill'], lw=1.8, zorder=4))
        ax.plot([ch_cx-1.2, ch_cx-0.2, ch_cx+1.8], [ch_cy-0.4, ch_cy-1.2, ch_cy+1.2], color=c['border'], lw=1.8, solid_capstyle='round', zorder=5)

    # 4. TEXT RENDERING
    text_x = bx + 5.5
    title_start_y = by + box_h - 48
    
    # Titles using sans-serif Arial-like font, matching sizes exactly
    ax.text(text_x, title_start_y, c['stage'], color=c['border'], fontfamily='sans-serif', 
            fontweight='bold', fontsize=20, ha='left', va='top', zorder=4)
    ax.text(text_x, title_start_y - 12, c['title'], color=c['border'], fontfamily='sans-serif', 
            fontweight='bold', fontsize=20, ha='left', va='top', zorder=4, linespacing=1.05)
    
    # Bullets (Using dark text like in the reference)
    bullets_start_y = title_start_y - 40
    cur_y = bullets_start_y
    for btext in c['bullets']:
        lines = btext.split('\n')
        # Bullet dash
        ax.text(text_x, cur_y, lines[0], color='#1C1E23', fontfamily='sans-serif', 
                fontsize=14.5, ha='left', va='top', zorder=4)
        cur_y -= 8.5
        for l in lines[1:]:
            ax.text(text_x, cur_y, l, color='#1C1E23', fontfamily='sans-serif', 
                    fontsize=14.5, ha='left', va='top', zorder=4)
            cur_y -= 8.5
        cur_y -= 2.5 # space between bullets

plt.subplots_adjust(left=0, right=1, top=1, bottom=0)
filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/methodology_stages_v3_twotone.png'
plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0.01)
print(f"Done saving {filename}")
