#!/usr/bin/env python3
"""
Generate the Server Lifecycle / Execution Flow Diagram for Section 5.1.1
Cart-OnePiece Thesis - Simulation Environment Configuration
(Revised: updated annotation, removed internal title — caption handled by LaTeX)
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch

fig, ax = plt.subplots(1, 1, figsize=(14, 4.5), dpi=200)
ax.set_xlim(0, 14)
ax.set_ylim(0, 4.5)
ax.axis('off')
fig.patch.set_facecolor('white')

# ─── Color Palette ─────────────────────────────────────
BG = '#F8F9FA'
BORDER = '#37474F'

COLORS = [
    ('#1B5E20', '#C8E6C9'),  # Step 1: Launch Server
    ('#0D47A1', '#BBDEFB'),  # Step 2: Wait Init
    ('#4A148C', '#E1BEE7'),  # Step 3: Load Map
    ('#E65100', '#FFE0B2'),  # Step 4: Spawn Traffic
    ('#1565C0', '#B3E5FC'),  # Step 5: Connect Client
    ('#2E7D32', '#DCEDC8'),  # Step 6: Run Experiment
    ('#B71C1C', '#FFCDD2'),  # Step 7: Cleanup
]

STEPS = [
    ('Launch\nCARLA Server', 'Offscreen Mode\nPort Config'),
    ('Wait for\nInitialization', '~15 seconds\nServer Readiness'),
    ('Load Map', 'Town Selection\nvia Python API'),
    ('Spawn\nTraffic', 'Vehicles &\nPedestrians'),
    ('Connect\nC++ Client', 'CARLA C++ API\nSync Mode'),
    ('Run\nExperiment', 'Pipeline\nExecution'),
    ('Cleanup &\nShutdown', 'Kill Server\n& Processes'),
]

# ─── Background ──────────────────────────────────────
bg = FancyBboxPatch((0.15, 0.15), 13.7, 4.2, boxstyle="round,pad=0.1",
                     fc=BG, ec='#B0BEC5', lw=1.5)
ax.add_patch(bg)

# ─── Draw steps ──────────────────────────────────────
box_w = 1.55
box_h = 2.2
gap = 0.25
start_x = 0.45
y_center = 2.0

for i, (title, subtitle) in enumerate(STEPS):
    x = start_x + i * (box_w + gap)
    header_color, body_color = COLORS[i]
    
    # Header
    header_h = 0.8
    header = FancyBboxPatch((x, y_center + box_h/2 - header_h), box_w, header_h,
                             boxstyle="round,pad=0.08", fc=header_color, ec=header_color, lw=1.5)
    ax.add_patch(header)
    ax.text(x + box_w/2, y_center + box_h/2 - header_h/2, title,
            fontsize=8, fontweight='bold', ha='center', va='center',
            color='white', fontfamily='sans-serif')
    
    # Body
    body = FancyBboxPatch((x, y_center - box_h/2), box_w, box_h - header_h,
                           boxstyle="round,pad=0.08", fc=body_color, ec=header_color, lw=1.5)
    ax.add_patch(body)
    ax.text(x + box_w/2, y_center - box_h/2 + (box_h - header_h)/2, subtitle,
            fontsize=7.5, ha='center', va='center', color='#333',
            fontfamily='sans-serif')
    
    # Step number circle
    circle = plt.Circle((x + box_w/2, y_center + box_h/2 + 0.35), 0.22,
                          fc=header_color, ec='white', lw=2, zorder=5)
    ax.add_patch(circle)
    ax.text(x + box_w/2, y_center + box_h/2 + 0.35, str(i+1),
            fontsize=9, fontweight='bold', ha='center', va='center',
            color='white', fontfamily='sans-serif', zorder=6)
    
    # Arrow to next step
    if i < len(STEPS) - 1:
        ax.annotate('', 
                     xy=(x + box_w + 0.05, y_center),
                     xytext=(x + box_w + gap - 0.05, y_center),
                     arrowprops=dict(arrowstyle='<-', color='#546E7A', lw=2.0, 
                                    mutation_scale=16))

# ─── Script label (updated) ──────────────────────────
ax.text(7.0, 0.35, 'Orchestrated by run_pipeline.sh  •  Configurable via CLI parameters',
        fontsize=9, ha='center', va='center', color='#78909C',
        fontstyle='italic', fontfamily='sans-serif',
        bbox=dict(boxstyle='round,pad=0.3', fc='white', ec='#B0BEC5', lw=1))

plt.tight_layout()
output_path = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/Server_Lifecycle_Flow.png'
plt.savefig(output_path, dpi=200, bbox_inches='tight', facecolor='white')
print(f"Saved: {output_path}")
plt.close()
