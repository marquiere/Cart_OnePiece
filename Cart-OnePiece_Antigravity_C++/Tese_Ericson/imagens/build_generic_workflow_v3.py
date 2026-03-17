import matplotlib.pyplot as plt
import matplotlib.patches as patches

# -----------------------------------------------------------------------------
# 1. MATPLOTLIB PNG GENERATION (Styled like reference)
# -----------------------------------------------------------------------------
fig_w, fig_h = 16, 12
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(5, 125)
ax.set_ylim(-10, 85)
ax.axis('off')

# Title
ax.text(65, 82, "Generic CArt-OnePiece Workflow", fontsize=24, fontweight='bold', ha='center', va='center', fontfamily='sans-serif', color='#2D3748')

# Grid Background (Light dashed lines for academic paper style)
for y in range(0, 90, 20):
    ax.axhline(y, color='#E2E8F0', lw=0.5, linestyle='--', zorder=0)
for x in range(10, 130, 25):
    ax.axvline(x, color='#E2E8F0', lw=0.5, linestyle='--', zorder=0)

def draw_box(ax, x, y, w, h, title_line1, title_line2, body_text, ec, tc):
    # Main Box (White fill, colored border)
    box = patches.FancyBboxPatch((x - w/2, y - h/2), w, h, boxstyle="round,pad=0.2,rounding_size=1.5", ec=ec, fc='white', lw=1.5, zorder=2)
    ax.add_patch(box)
    
    # Internal separator line
    line_y = y + h/2 - h*0.35 + 1
    ax.plot([x - w/2, x + w/2], [line_y, line_y], color=ec, lw=0.8, alpha=0.5, zorder=3)

    # Title Text (Colored)
    ax.text(x, line_y + (h*0.35)/2 + 0.5, title_line1, ha='center', va='center', fontsize=12, fontweight='bold', color=tc, fontfamily='sans-serif', zorder=4)
    if title_line2:
        ax.text(x, line_y + (h*0.35)/2 - 2.5, title_line2, ha='center', va='center', fontsize=12, fontweight='bold', color=tc, fontfamily='sans-serif', zorder=4)
    
    # Body Text (Bullets) (Dark Gray)
    ax.text(x - w/2 + 1.5, line_y - 2.5, body_text, ha='left', va='top', fontsize=9.5, color='#4A5568', fontfamily='sans-serif', zorder=4, linespacing=1.6)

def draw_title_only_box(ax, x, y, w, h, title_line1, title_line2, ec, tc):
    # Main Box (White fill, colored border)
    box = patches.FancyBboxPatch((x - w/2, y - h/2), w, h, boxstyle="round,pad=0.2,rounding_size=1.5", ec=ec, fc='white', lw=1.5, zorder=2)
    ax.add_patch(box)

    # Title Text
    ax.text(x, y + 1.5, title_line1, ha='center', va='center', fontsize=12, fontweight='bold', color=tc, fontfamily='sans-serif', zorder=4)
    if title_line2:
        ax.text(x, y - 1.5, title_line2, ha='center', va='center', fontsize=12, fontweight='bold', color=tc, fontfamily='sans-serif', zorder=4)

def draw_arrow(ax, x1, y1, x2, y2, color, lw=2, style="-|>", ls="-"):
    ax.annotate("", xy=(x2, y2), xytext=(x1, y1), arrowprops=dict(arrowstyle=style, color=color, lw=lw, ls=ls, shrinkA=0, shrinkB=0, joinstyle='round'))
    
def draw_elbow_arrow(ax, x1, y1, x2, y2, color, lw=2):
    ax.plot([x1, x1], [y1, y2], color=color, lw=lw)
    draw_arrow(ax, x1, y2, x2, y2, color, lw)


# Colors matching the reference image
c_blue = {'ec': '#63B3ED', 'tc': '#4299E1'}   # Light Blue
c_green = {'ec': '#68D391', 'tc': '#48BB78'}  # Light Green
c_purple = {'ec': '#9F7AEA', 'tc': '#805AD5'} # Light Purple
c_gray = {'ec': '#A0AEC0', 'tc': '#4A5568'}   # Slate
c_red = {'ec': '#FC8181', 'tc': '#F56565'}    # Light Red
c_orange = {'ec': '#FBD38D', 'tc': '#ED8936'} # Light Orange

w, h = 42, 12
space_y = 6
xc = 65

y1 = 68
y2 = y1 - h - space_y
y3 = y2 - h - space_y
y4 = y3 - h - space_y * 1.5

# Box 1
b1_t1 = "Simulation Environment"
b1_t2 = "Configuration"
b1_body = "• Scenario definition and CARLA server execution\n• Ego-vehicle spawn and dynamic actor configuration\n• Definition of the ADV sensor suite"
draw_box(ax, xc, y1, w, h, b1_t1, b1_t2, b1_body, **c_blue)

# Box 2
b2_t1 = "Sensor Data"
b2_t2 = "Acquisition"
b2_body = "• Collection of active sensor streams\n• Temporal synchronization and buffering\n• Data preparation for task-based execution"
draw_box(ax, xc, y2, w, h, b2_t1, b2_t2, b2_body, **c_green)

# Box 3
b3_t1 = "ADV Workload and"
b3_t2 = "Task Execution"
b3_body = "• Generation of task graphs from sensor data\n• Task submission for CPU/GPU heterogeneous scheduling\n• Execution of modular perception, fusion, and control tasks"
draw_box(ax, xc, y3, w, h, b3_t1, b3_t2, b3_body, **c_purple)

# Side Box 1 (Left)
ws, hs = 18, 8
xs1 = xc - w/2 - ws/2 - 6
ts1_1 = "User-Defined"
ts1_2 = "Scheduler"
draw_title_only_box(ax, xs1, y3, ws, hs, ts1_1, ts1_2, **c_gray)

# Side Box 2 (Right)
xs2 = xc + w/2 + ws/2 + 6
ts2_1 = "User-Defined"
ts2_2 = "Processing Module"
draw_title_only_box(ax, xs2, y3, ws, hs, ts2_1, ts2_2, **c_red)

# Box 4 (Branch Left)
x4 = xc - w/2 + 6
b4_t1 = "Performance and"
b4_t2 = "Analysis Outputs"
b4_body = "• Runtime latency profiling and trace generation\n• Resource utilization and workload characterization\n• Comparative evaluation of scheduling policies"
draw_box(ax, x4, y4, w - 8, h, b4_t1, b4_t2, b4_body, **c_gray)

# Box 5 (Branch Right)
x5 = xc + w/2 - 6
b5_t1 = "Synthetic Dataset"
b5_t2 = "Generation"
b5_body = "• Synchronized export of sensor data and ground truth\n• Generation of reproducible annotated ADV datasets\n• Artifacts for offline model training and validation"
draw_box(ax, x5, y4, w - 8, h, b5_t1, b5_t2, b5_body, **c_orange)

# Vertical Arrows
draw_arrow(ax, xc, y1 - h/2, xc, y2 + h/2, c_blue['ec'])
draw_arrow(ax, xc, y2 - h/2, xc, y3 + h/2, c_green['ec'])

# Horizontal Side Arrows
draw_arrow(ax, xs1 + ws/2, y3, xc - w/2, y3, c_gray['ec'])
draw_arrow(ax, xs2 - ws/2, y3, xc + w/2, y3, c_red['ec'])

# Branching arrows (Elbow shape)
branch_y_start = y3 - h/2
branch_y_mid = y3 - h/2 - (space_y * 1.5)/2

# Draw main trunk down
ax.plot([xc, xc], [branch_y_start, branch_y_mid], color=c_purple['ec'], lw=2)
# Draw horizontal bar
ax.plot([x4, x5], [branch_y_mid, branch_y_mid], color=c_purple['ec'], lw=2)

# Drop down to Left box
draw_arrow(ax, x4, branch_y_mid, x4, y4 + h/2, c_gray['ec'])
# Drop down to Right box
draw_arrow(ax, x5, branch_y_mid, x5, y4 + h/2, c_orange['ec'])

plt.subplots_adjust(left=0.01, right=0.99, top=0.99, bottom=0.01)
png_filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/generic_workflow_v3.png'
plt.savefig(png_filename, dpi=300, bbox_inches='tight', pad_inches=0.1)
print(f"Done saving {png_filename}")

# -----------------------------------------------------------------------------
# 2. LATEX TIKZ GENERATION (Styled like reference)
# -----------------------------------------------------------------------------
tex_filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/generic_workflow_v3.tex'
tex_content = r"""\documentclass[tikz,border=10pt]{standalone}
\usepackage{helvet}
\renewcommand{\familydefault}{\sfdefault}
\usetikzlibrary{positioning, arrows.meta, backgrounds, calc}

\begin{document}
\begin{tikzpicture}[
    node distance=1cm and 0.5cm,
    >={Stealth[length=3mm, width=2mm]},
    box/.style={
        rectangle,
        draw=#1,
        fill=white,
        thick,
        rounded corners=4pt,
        align=left,
        text width=8.5cm,
        minimum height=3.5cm,
        inner sep=0pt
    },
    titlebox/.style={
        rectangle,
        draw=#1,
        fill=white,
        thick,
        rounded corners=4pt,
        align=center,
        text width=3.5cm,
        minimum height=2cm,
        font=\bfseries\color{#1},
        inner sep=8pt
    },
    arrow/.style={
        ->,
        thick,
        draw=#1
    },
    line/.style={
        thick,
        draw=#1
    }
]

% Colors
\definecolor{blue_color}{HTML}{4299E1}
\definecolor{green_color}{HTML}{48BB78}
\definecolor{purple_color}{HTML}{805AD5}
\definecolor{slate_color}{HTML}{4A5568}
\definecolor{red_color}{HTML}{F56565}
\definecolor{orange_color}{HTML}{ED8936}

% Grid Background (Optional for standalone, but adds the academic feel)
\begin{scope}[on background layer]
    \draw[black!5, dashed, step=2cm] (-7,-9) grid (7,4);
\end{scope}

% Title
\node[font=\Large\bfseries\color{black!80}, align=center, yshift=0.5cm] at (0, 3.5) {Generic CArt-OnePiece Workflow};

% Box 1
\node[box=blue_color] (B1) at (0,1.5) {};
\node[anchor=north, font=\bfseries\color{blue_color}, align=center, yshift=-0.2cm] at (B1.north) {Simulation Environment\\[-0.5ex]Configuration};
\draw[blue_color!50, thick] ([yshift=-1.2cm]B1.north west) -- ([yshift=-1.2cm]B1.north east);
\node[anchor=north west, font=\small\linespread{1.1}\selectfont\color{black!80}, align=left, yshift=-1.3cm, xshift=0.2cm] at (B1.north west) {
    \begin{minipage}{8.1cm}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Scenario definition and CARLA server execution
        \item Ego-vehicle spawn and dynamic actor configuration
        \item Definition of the ADV sensor suite
    \end{itemize}
    \end{minipage}
};

% Box 2
\node[box=green_color, below=of B1] (B2) {};
\node[anchor=north, font=\bfseries\color{green_color}, align=center, yshift=-0.2cm] at (B2.north) {Sensor Data\\[-0.5ex]Acquisition};
\draw[green_color!50, thick] ([yshift=-1.2cm]B2.north west) -- ([yshift=-1.2cm]B2.north east);
\node[anchor=north west, font=\small\linespread{1.1}\selectfont\color{black!80}, align=left, yshift=-1.3cm, xshift=0.2cm] at (B2.north west) {
    \begin{minipage}{8.1cm}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Collection of active sensor streams
        \item Temporal synchronization and buffering
        \item Data preparation for task-based execution
    \end{itemize}
    \end{minipage}
};

% Box 3
\node[box=purple_color, below=of B2] (B3) {};
\node[anchor=north, font=\bfseries\color{purple_color}, align=center, yshift=-0.2cm] at (B3.north) {ADV Workload and\\[-0.5ex]Task Execution};
\draw[purple_color!50, thick] ([yshift=-1.2cm]B3.north west) -- ([yshift=-1.2cm]B3.north east);
\node[anchor=north west, font=\small\linespread{1.1}\selectfont\color{black!80}, align=left, yshift=-1.3cm, xshift=0.2cm] at (B3.north west) {
    \begin{minipage}{8.1cm}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Generation of task graphs from sensor data
        \item Task submission for CPU/GPU heterogeneous scheduling
        \item Execution of modular perception, fusion, and control tasks
    \end{itemize}
    \end{minipage}
};

% Side Box 1 (Left)
\node[titlebox=slate_color, left=of B3, xshift=-0.5cm] (S1) {User-Defined\\[-0.5ex]Scheduler};

% Side Box 2 (Right)
\node[titlebox=red_color, right=of B3, xshift=0.5cm] (S2) {User-Defined\\[-0.5ex]Processing Module};

% Box 4
\node[box=slate_color, below left=of B3, text width=7.5cm, xshift=1.5cm, yshift=-0.5cm] (B4) {};
\node[anchor=north, font=\bfseries\color{slate_color}, align=center, yshift=-0.2cm] at (B4.north) {Performance and\\[-0.5ex]Analysis Outputs};
\draw[slate_color!50, thick] ([yshift=-1.2cm]B4.north west) -- ([yshift=-1.2cm]B4.north east);
\node[anchor=north west, font=\small\linespread{1.1}\selectfont\color{black!80}, align=left, yshift=-1.3cm, xshift=0.2cm] at (B4.north west) {
    \begin{minipage}{7.1cm}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Runtime latency profiling and trace generation
        \item Resource utilization and workload characterization
        \item Comparative evaluation of scheduling policies
    \end{itemize}
    \end{minipage}
};

% Box 5
\node[box=orange_color, below right=of B3, text width=7.5cm, xshift=-1.5cm, yshift=-0.5cm] (B5) {};
\node[anchor=north, font=\bfseries\color{orange_color}, align=center, yshift=-0.2cm] at (B5.north) {Synthetic Dataset\\[-0.5ex]Generation};
\draw[orange_color!50, thick] ([yshift=-1.2cm]B5.north west) -- ([yshift=-1.2cm]B5.north east);
\node[anchor=north west, font=\small\linespread{1.1}\selectfont\color{black!80}, align=left, yshift=-1.3cm, xshift=0.2cm] at (B5.north west) {
    \begin{minipage}{7.1cm}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Synchronized export of sensor data and ground truth
        \item Generation of reproducible annotated ADV datasets
        \item Artifacts for offline model training and validation
    \end{itemize}
    \end{minipage}
};

% Arrows
\draw[arrow=blue_color] (B1.south) -- (B2.north);
\draw[arrow=green_color] (B2.south) -- (B3.north);

\draw[arrow=slate_color] (S1.east) -- (B3.west);
\draw[arrow=red_color] (S2.west) -- (B3.east);

% Branching Arrows
\coordinate (split) at ($(B3.south) + (0, -0.7cm)$);
\draw[line=purple_color] (B3.south) -- (split);
\draw[line=purple_color] (B4.north |- split) -- (B5.north |- split);
\draw[arrow=slate_color] (B4.north |- split) -- (B4.north);
\draw[arrow=orange_color] (B5.north |- split) -- (B5.north);

\end{tikzpicture}
\end{document}
"""
with open(tex_filename, 'w') as f:
    f.write(tex_content)
print(f"Done saving {tex_filename}")
