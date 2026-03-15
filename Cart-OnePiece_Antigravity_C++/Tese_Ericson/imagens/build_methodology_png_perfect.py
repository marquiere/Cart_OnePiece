import matplotlib.pyplot as plt
import matplotlib.patches as patches
import math

fig_w, fig_h = 12.0, 7.8  # Further refining aspect ratio
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(0, 340)
ax.set_ylim(0, 220)
ax.axis('off')

# Adding a soft blueish grid background to match the reference's graph paper look
for x in range(0, 360, 10):
    ax.plot([x, x], [0, 240], color='#EAF0F6', lw=0.6, zorder=-1)
for y in range(0, 240, 10):
    ax.plot([0, 360], [y, y], color='#EAF0F6', lw=0.6, zorder=-1)

colors = [
    {'border': '#33719C', 'top': '#EAF3FA', 'bot': '#FFFFFF', 'stage': 'Stage 1:', 'title': 'Systematic\nMapping Study', 
     'bullets': ['- Collect and\n  analyze existing\n  literature', '- Identify key\n  research trends\n  and gaps', '- Classify prior\n  work']},
    {'border': '#4C8A55', 'top': '#EBF5ED', 'bot': '#FFFFFF', 'stage': 'Stage 2:', 'title': 'Experimental\nFeasibility',
     'bullets': ['- Develop initial\n  prototypes', '- Conduct\n  proof-of-concept\n  experiments', '- Assess\n  technical\n  viability']},
    {'border': '#C78C30', 'top': '#FDF5EB', 'bot': '#FFFFFF', 'stage': 'Stage 3:', 'title': 'Cart-OnePiece\nFramework',
     'bullets': ['- Propose new\n  theoretical\n  model', '- Implement the\n  Cart-OnePiece\n  system', '- Integrate\n  components\n  and protocols']},
    {'border': '#B74246', 'top': '#FBEBEC', 'bot': '#FFFFFF', 'stage': 'Stage 4:', 'title': 'Empirical\nEvaluation',
     'bullets': ['- Execute\n  controlled\n  experiments', '- Compare with\n  baseline\n  methods', '- Analyze\n  quantitative\n  performance\n  data']}
]

box_w = 68
box_h = 190
gap = 9
start_x = 16
start_y = 15

def draw_gear(ax, cx, cy, radius, color):
    ax.add_patch(patches.Circle((cx, cy), radius * 0.65, edgecolor=color, facecolor='none', lw=1.6, zorder=3))
    for angle in range(0, 360, 45):
        rad = angle * math.pi / 180
        ax.plot([cx + radius*0.75*math.cos(rad), cx + radius*1.1*math.cos(rad)],
                [cy + radius*0.75*math.sin(rad), cy + radius*1.1*math.sin(rad)], color=color, lw=2.2, solid_capstyle='butt', zorder=3)
    ax.add_patch(patches.Circle((cx, cy), radius * 0.85, edgecolor=color, facecolor='none', lw=1.6, zorder=3))

for i, c in enumerate(colors):
    bx = start_x + i * (box_w + gap)
    by = start_y
    cx = bx + box_w / 2
    
    # Arrows
    if i > 0:
        arr_lx = start_x + (i-1)*(box_w+gap) + box_w + 1.0
        arr_rx = bx - 1.0
        arr_y = by + box_h/2.0 + 4
        c_arrow = '#5B93C4' # Softened overall arrow color - they all look blue/greenish but varied in ref - actually the ref has stage colors. Let's stick to stage previous
        # In original reference, arrow from Stage1->2 is blue, 2->3 is green, 3->4 is orange.
        c_arrow_border = colors[i-1]['border']
        ax.plot([arr_lx, arr_rx], [arr_y, arr_y], color=c_arrow_border, lw=1.3, zorder=1)
        ah_len = 3.2
        ah_wid = 2.4
        ax.plot([arr_rx - ah_len, arr_rx], [arr_y + ah_wid, arr_y], color=c_arrow_border, lw=1.3, solid_capstyle='round', solid_joinstyle='round', zorder=1)
        ax.plot([arr_rx - ah_len, arr_rx], [arr_y - ah_wid, arr_y], color=c_arrow_border, lw=1.3, solid_capstyle='round', solid_joinstyle='round', zorder=1)

    # Box Background Split
    split_y = by + box_h - 70 # Split exactly below the title area
    
    clip_box = patches.FancyBboxPatch((bx, by), box_w, box_h, boxstyle="round,pad=0,rounding_size=5", lw=0, zorder=0)
    ax.add_patch(clip_box)
    
    bot_rect = patches.Rectangle((bx-1, by-1), box_w+2, split_y-by+1, facecolor=c['bot'], zorder=1)
    bot_rect.set_clip_path(clip_box)
    ax.add_patch(bot_rect)
    
    top_rect = patches.Rectangle((bx-1, split_y), box_w+2, by+box_h-split_y+1, facecolor=c['top'], zorder=1)
    top_rect.set_clip_path(clip_box)
    ax.add_patch(top_rect)
    
    # Divider Line
    ax.plot([bx, bx+box_w], [split_y, split_y], color=c['border'], lw=0.6, alpha=0.3, zorder=2)
    
    # Outer Border
    outer_border = patches.FancyBboxPatch((bx, by), box_w, box_h, boxstyle="round,pad=0,rounding_size=5", 
                                        ec=c['border'], fc='none', lw=1.3, zorder=3)
    ax.add_patch(outer_border)
    
    # ICONS
    icon_cy = by + box_h - 22
    if i == 0:
        rad = 5.2
        m_cx, m_cy = cx - 2, icon_cy + 3.5
        ax.add_patch(patches.Circle((m_cx, m_cy), rad, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.plot([m_cx + 3.8, m_cx + 9.0], [m_cy - 3.8, m_cy - 9.0], color=c['border'], lw=2.4, solid_capstyle='round', zorder=3)
    elif i == 1:
        draw_gear(ax, cx - 6, icon_cy + 5.5, 3.8, c['border'])
        draw_gear(ax, cx - 10.5, icon_cy - 0, 2.3, c['border'])
        fx, fy = cx + 8, icon_cy - 4
        fw, fh = 10, 11
        flask_points = [[fx, fy+fh], [fx, fy+fh-3], [fx-fw/2, fy], [fx+fw/2, fy], [fx, fy+fh-3]]
        poly = patches.Polygon(flask_points, closed=True, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3, joinstyle='round')
        ax.add_patch(poly)
        ax.plot([fx-fw*.38, fx+fw*.38], [fy+fh*.28, fy+fh*.28], color=c['border'], lw=1.5, zorder=3)
    elif i == 2:
        sw, sh = 5.0, 3.6
        def box_node(nx, ny):
            ax.add_patch(patches.Rectangle((nx-sw/2, ny-sh/2), sw, sh, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        box_node(cx, icon_cy + 7.5)
        box_node(cx - 7.5, icon_cy - 2)
        box_node(cx + 7.5, icon_cy - 2)
        box_node(cx, icon_cy - 2)
        ax.plot([cx, cx], [icon_cy+7.5-sh/2, icon_cy-2+sh/2], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx-7.5, cx+7.5], [icon_cy+2.8, icon_cy+2.8], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx-7.5, cx-7.5], [icon_cy+2.8, icon_cy-2+sh/2], color=c['border'], lw=1.8, zorder=3)
        ax.plot([cx+7.5, cx+7.5], [icon_cy+2.8, icon_cy-2+sh/2], color=c['border'], lw=1.8, zorder=3)
    elif i == 3:
        w = 2.4
        base_y = icon_cy - 4.5
        ax.add_patch(patches.Rectangle((cx-7.5, base_y), w, 4, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.add_patch(patches.Rectangle((cx-3, base_y), w, 8, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.add_patch(patches.Rectangle((cx+1.5, base_y), w, 12, edgecolor=c['border'], facecolor='none', lw=1.8, zorder=3))
        ax.plot([cx-9.5, cx+6], [base_y, base_y], color=c['border'], lw=1.8, zorder=3)
        ch_cx, ch_cy = cx+6.5, base_y+2
        ax.add_patch(patches.Circle((ch_cx, ch_cy), 2.8, edgecolor=c['border'], facecolor='white', lw=1.8, zorder=4))
        ax.plot([ch_cx-1.0, ch_cx-0.2, ch_cx+1.5], [ch_cy-0.3, ch_cy-1.0, ch_cy+1.0], color=c['border'], lw=1.8, zorder=5)

    # TEXT TITLE
    text_x = bx + 5.0
    title_start_y = by + box_h - 43
    
    ax.text(text_x, title_start_y, c['stage'], color=c['border'], family='sans-serif', 
            weight='bold', fontsize=13.5, ha='left', va='top', zorder=4)
    ax.text(text_x, title_start_y - 6.5, c['title'], color=c['border'], family='sans-serif', 
            weight='bold', fontsize=13.5, ha='left', va='top', zorder=4, linespacing=0.98)
    
    # Bullets TEXT
    bullets_start_y = split_y - 8
    cur_y = bullets_start_y
    for btext in c['bullets']:
        lines = btext.split('\n')
        ax.text(text_x, cur_y, lines[0], color='#2B2D31', family='sans-serif', 
                fontsize=10.0, ha='left', va='top', zorder=4)
        cur_y -= 4.8
        for l in lines[1:]:
            # Removing extra indent, the user specifically mentioned: "Avoid overly large left indentation."
            ax.text(text_x + 3.0, cur_y, l.strip(), color='#2B2D31', family='sans-serif', 
                    fontsize=10.0, ha='left', va='top', zorder=4)
            cur_y -= 4.8
        cur_y -= 2.0 

plt.subplots_adjust(left=0, right=1, top=1, bottom=0)
filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/methodology_stages_perfect.png'
plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0.01)
print(f"Done saving {filename}")
