#!/usr/bin/env python3
"""
Generate the Client-Server Architecture Diagram for Section 5.1.1
Cart-OnePiece Thesis - Simulation Environment Configuration
(Revised: GUI box restored, internal title removed — caption handled by LaTeX)
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import numpy as np

fig, ax = plt.subplots(1, 1, figsize=(14, 8), dpi=200)
ax.set_xlim(0, 14)
ax.set_ylim(0, 8)
ax.axis('off')
fig.patch.set_facecolor('white')

# ─── Color Palette ─────────────────────────────────────
BG_LIGHT = '#F0F4F8'
BORDER_DARK = '#2C3E50'

SERVER_FILL = '#E8F5E9'
SERVER_BORDER = '#2E7D32'
SERVER_HEADER = '#2E7D32'

CLIENT_FILL = '#E3F2FD'
CLIENT_BORDER = '#1565C0'
CLIENT_HEADER = '#1565C0'

TRAFFIC_FILL = '#FFF3E0'
TRAFFIC_BORDER = '#E65100'
TRAFFIC_HEADER = '#E65100'

GUI_FILL = '#F3E5F5'
GUI_BORDER = '#6A1B9A'
GUI_HEADER = '#6A1B9A'

ARROW_COLOR = '#455A64'

# ─── Background ──────────────────────────────────────
bg = FancyBboxPatch((0.2, 0.2), 13.6, 7.6, boxstyle="round,pad=0.1",
                     fc=BG_LIGHT, ec='#B0BEC5', lw=1.5)
ax.add_patch(bg)

# ─── Helper: draw a two-tone box ──────────────────────
def draw_box(ax, x, y, w, h, title, items, fill, border, header_color):
    header_h = 0.5
    # Header bar
    header = FancyBboxPatch((x, y + h - header_h), w, header_h,
                             boxstyle="round,pad=0.05", fc=header_color, ec=border, lw=1.5)
    ax.add_patch(header)
    ax.text(x + w/2, y + h - header_h/2, title,
            fontsize=11, fontweight='bold', ha='center', va='center',
            color='white', fontfamily='sans-serif')
    # Body
    body = FancyBboxPatch((x, y), w, h - header_h,
                           boxstyle="round,pad=0.05", fc=fill, ec=border, lw=1.5)
    ax.add_patch(body)
    # Items
    for i, item in enumerate(items):
        ax.text(x + 0.25, y + h - header_h - 0.4 - i * 0.4, f"• {item}",
                fontsize=8.5, ha='left', va='center', color='#333',
                fontfamily='sans-serif')

# ─── CARLA Server Box ─────────────────────────────────
draw_box(ax, 0.5, 3.5, 4.5, 3.2,
         'CARLA Server (UE4)',
         ['Physics & World Simulation',
          'Scene Rendering (Offscreen)',
          'Map Management',
          'Sensor Data Generation',
          'Vehicle Dynamics'],
         SERVER_FILL, SERVER_BORDER, SERVER_HEADER)

# ─── C++ Client Application Box ──────────────────────
draw_box(ax, 8.5, 3.5, 5.0, 3.2,
         'C++ Client Application (StarPU)',
         ['CARLA C++ API Connection',
          'Map Loading & World Config',
          'Ego Vehicle Spawning',
          'Sensor Attachment',
          'ADV Pipeline Execution'],
         CLIENT_FILL, CLIENT_BORDER, CLIENT_HEADER)

# ─── Traffic Manager Box ─────────────────────────────
draw_box(ax, 0.5, 0.5, 4.5, 2.4,
         'Traffic Manager (Python API)',
         ['Vehicle Spawning (N configurable)',
          'Pedestrian Spawning (W configurable)',
          'Lane Following & Collision Avoidance',
          'Asynchronous Actor Management'],
         TRAFFIC_FILL, TRAFFIC_BORDER, TRAFFIC_HEADER)

# ─── GUI Launcher Box ─────────────────────────────────
draw_box(ax, 8.5, 0.5, 5.0, 2.4,
         'GUI Launcher (CustomTkinter)',
         ['Map Selection (Town01–Town10)',
          'Vehicle & Pedestrian Count',
          'Server Connection Parameters',
          'Execution Mode Selection'],
         GUI_FILL, GUI_BORDER, GUI_HEADER)

# ─── Arrows ──────────────────────────────────────────
# Server ↔ Client (bidirectional)
ax.annotate('', xy=(8.4, 5.4), xytext=(5.1, 5.4),
            arrowprops=dict(arrowstyle='->', color='#1565C0', lw=2.5, mutation_scale=20))
ax.annotate('', xy=(5.1, 4.7), xytext=(8.4, 4.7),
            arrowprops=dict(arrowstyle='->', color='#2E7D32', lw=2.5, mutation_scale=20))

# Labels on arrows
ax.text(6.75, 5.65, 'Commands / Map Loading',
        fontsize=8, ha='center', va='center', color='#1565C0',
        fontweight='bold', fontfamily='sans-serif',
        bbox=dict(boxstyle='round,pad=0.2', fc='white', ec='#1565C0', alpha=0.9))
ax.text(6.75, 4.4, 'Sensor Data / World State',
        fontsize=8, ha='center', va='center', color='#2E7D32',
        fontweight='bold', fontfamily='sans-serif',
        bbox=dict(boxstyle='round,pad=0.2', fc='white', ec='#2E7D32', alpha=0.9))

# Server ↔ Traffic Manager (vertical)
ax.annotate('', xy=(2.75, 3.0), xytext=(2.75, 3.4),
            arrowprops=dict(arrowstyle='<->', color=TRAFFIC_BORDER, lw=2, mutation_scale=18))
ax.text(4.0, 3.2, 'Actor\nSpawning',
        fontsize=7.5, ha='center', va='center', color=TRAFFIC_BORDER,
        fontweight='bold', fontfamily='sans-serif')

# GUI → Client (vertical)
ax.annotate('', xy=(11.0, 3.4), xytext=(11.0, 3.0),
            arrowprops=dict(arrowstyle='->', color=GUI_BORDER, lw=2, mutation_scale=18))
ax.text(12.2, 3.2, 'Configuration\nParameters',
        fontsize=7.5, ha='center', va='center', color=GUI_BORDER,
        fontweight='bold', fontfamily='sans-serif')

# GUI → Traffic Manager (horizontal, bottom)
ax.annotate('', xy=(5.1, 1.7), xytext=(8.4, 1.7),
            arrowprops=dict(arrowstyle='->', color='#795548', lw=1.8,
                           connectionstyle='arc3,rad=0.0', mutation_scale=16))
ax.text(6.75, 1.95, 'Traffic Parameters',
        fontsize=7.5, ha='center', va='center', color='#795548',
        fontweight='bold', fontfamily='sans-serif',
        bbox=dict(boxstyle='round,pad=0.15', fc='white', ec='#795548', alpha=0.9))

# ─── Protocol label ──────────────────────────────────
ax.text(6.75, 6.2, 'TCP/IP RPC (Port 2000)',
        fontsize=9, ha='center', va='center', color=BORDER_DARK,
        fontweight='bold', fontfamily='sans-serif',
        bbox=dict(boxstyle='round,pad=0.3', fc='#ECEFF1', ec=BORDER_DARK, lw=1.2))

plt.tight_layout()
output_path = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/Client_Server_Architecture.png'
plt.savefig(output_path, dpi=200, bbox_inches='tight', facecolor='white')
print(f"Saved: {output_path}")
plt.close()
