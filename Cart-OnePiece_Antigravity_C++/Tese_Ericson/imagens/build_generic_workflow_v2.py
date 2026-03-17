import matplotlib.pyplot as plt
import matplotlib.patches as patches

# -----------------------------------------------------------------------------
# 1. MATPLOTLIB PNG GENERATION
# -----------------------------------------------------------------------------
fig_w, fig_h = 16, 17
fig = plt.figure(figsize=(fig_w, fig_h), dpi=300)
ax = fig.add_axes([0, 0, 1, 1], aspect='equal')
ax.set_xlim(0, 100)
ax.set_ylim(-5, 120)
ax.axis('off')

# Title
ax.text(50, 115, "Generic CArt-OnePiece Workflow", fontsize=20, fontweight='bold', ha='center', va='center', fontfamily='sans-serif', color='#2D3748')

def draw_box(ax, x, y, w, h, title_line1, title_line2, body_text, ec, fc, title_fc):
    # Main Box
    box = patches.FancyBboxPatch((x - w/2, y - h/2), w, h, boxstyle="round,pad=0.2,rounding_size=2", ec=ec, fc=fc, lw=2, zorder=2)
    ax.add_patch(box)
    
    # Title Header Box (Top portion)
    header_h = h * 0.35
    header_y = y + h/2 - header_h
    header = patches.FancyBboxPatch((x - w/2, header_y), w, header_h, boxstyle="round,pad=0.2,rounding_size=2", ec=ec, fc=title_fc, lw=2, zorder=3)
    ax.add_patch(header)
    
    # Square bottom of header
    rect = patches.Rectangle((x - w/2, header_y), w, header_h/2, ec='none', fc=title_fc, zorder=4)
    ax.add_patch(rect)
    ax.plot([x - w/2, x + w/2], [header_y, header_y], color=ec, lw=2, zorder=5)

    # Title Text
    ax.text(x, header_y + header_h/2 + 1.5, title_line1, ha='center', va='center', fontsize=12, fontweight='bold', color='white', fontfamily='sans-serif', zorder=6)
    ax.text(x, header_y + header_h/2 - 2.5, title_line2, ha='center', va='center', fontsize=12, fontweight='bold', color='white', fontfamily='sans-serif', zorder=6)
    
    # Body Text (Bullets)
    ax.text(x - w/2 + 2, header_y - 2, body_text, ha='left', va='top', fontsize=10.5, color='#2D3748', fontfamily='sans-serif', zorder=6, linespacing=1.6)

def draw_title_only_box(ax, x, y, w, h, title_line1, title_line2, ec, fc, title_fc):
    # Title Header Box Only
    header = patches.FancyBboxPatch((x - w/2, y - h/2), w, h, boxstyle="round,pad=0.2,rounding_size=2", ec=ec, fc=title_fc, lw=2, zorder=3)
    ax.add_patch(header)

    # Title Text
    ax.text(x, y + 2, title_line1, ha='center', va='center', fontsize=12, fontweight='bold', color='white', fontfamily='sans-serif', zorder=6)
    ax.text(x, y - 2, title_line2, ha='center', va='center', fontsize=12, fontweight='bold', color='white', fontfamily='sans-serif', zorder=6)

def draw_arrow(ax, x1, y1, x2, y2, color='#4A5568', lw=2.5, style="-|>", ls="-"):
    ax.annotate("", xy=(x2, y2), xytext=(x1, y1), arrowprops=dict(arrowstyle=style, color=color, lw=lw, ls=ls, shrinkA=0, shrinkB=0, joinstyle='round'))

# Colors defined by user
c1 = {'ec': '#2B6CB0', 'fc': '#EBF8FF', 'title_fc': '#3182CE'} # Blue
c2 = {'ec': '#276749', 'fc': '#F0FFF4', 'title_fc': '#38A169'} # Green
c3 = {'ec': '#6B46C1', 'fc': '#FAF5FF', 'title_fc': '#805AD5'} # Purple
c_side1 = {'ec': '#4A5568', 'fc': '#F7FAFC', 'title_fc': '#718096'} # Slate
c_side2 = {'ec': '#C53030', 'fc': '#FFF5F5', 'title_fc': '#E53E3E'} # Red
c4 = {'ec': '#4A5568', 'fc': '#F7FAFC', 'title_fc': '#718096'} # Slate
c5 = {'ec': '#C05621', 'fc': '#FFFAF0', 'title_fc': '#DD6B20'} # Orange

w, h = 42, 18
space_y = 6
xc = 50

y1 = 95
y2 = y1 - h - space_y
y3 = y2 - h - space_y
y4 = y3 - h - space_y * 1.5

# Box 1
b1_t1 = "Simulation Environment"
b1_t2 = "Configuration"
b1_body = "• Scenario definition and CARLA server execution\n• Ego-vehicle spawn and dynamic actor configuration\n• Definition of the ADV sensor suite"
draw_box(ax, xc, y1, w, h, b1_t1, b1_t2, b1_body, **c1)

# Box 2
b2_t1 = "Sensor Data"
b2_t2 = "Acquisition"
b2_body = "• Collection of active sensor streams\n• Temporal synchronization and buffering\n• Data preparation for task-based execution"
draw_box(ax, xc, y2, w, h, b2_t1, b2_t2, b2_body, **c2)

# Box 3
b3_t1 = "ADV Workload and"
b3_t2 = "Task Execution"
b3_body = "• Generation of task graphs from sensor data\n• Task submission for CPU/GPU heterogeneous scheduling\n• Execution of modular perception, fusion, and control tasks"
draw_box(ax, xc, y3, w+8, h+2, b3_t1, b3_t2, b3_body, **c3)

# Side Box 1 (Left)
xs1 = xc - w/2 - 16
ws = 22
hs = 8
ts1_1 = "User-Defined"
ts1_2 = "Scheduler"
draw_title_only_box(ax, xs1, y3, ws, hs, ts1_1, ts1_2, **c_side1)

# Side Box 2 (Right)
xs2 = xc + w/2 + 16
ts2_1 = "User-Defined"
ts2_2 = "Processing Module"
draw_title_only_box(ax, xs2, y3, ws, hs, ts2_1, ts2_2, **c_side2)

# Box 4 (Branch Left)
x4 = xc - 24
b4_t1 = "Performance and"
b4_t2 = "Analysis Outputs"
b4_body = "• Runtime latency profiling and trace generation\n• Resource utilization and workload characterization\n• Comparative evaluation of scheduling policies"
draw_box(ax, x4, y4, w-2, h, b4_t1, b4_t2, b4_body, **c4)

# Box 5 (Branch Right)
x5 = xc + 24
b5_t1 = "Synthetic Dataset"
b5_t2 = "Generation"
b5_body = "• Synchronized export of sensor data and ground truth\n• Generation of reproducible annotated ADV datasets\n• Artifacts for offline model training and validation"
draw_box(ax, x5, y4, w-2, h, b5_t1, b5_t2, b5_body, **c5)

# Arrows Central
draw_arrow(ax, xc, y1 - h/2, xc, y2 + h/2)
draw_arrow(ax, xc, y2 - h/2, xc, y3 + h/2 + 1)

# Arrows Side
draw_arrow(ax, xs1 + ws/2, y3, xc - w/2 - 4, y3)
draw_arrow(ax, xs2 - ws/2, y3, xc + w/2 + 4, y3)

# Branching arrows
draw_arrow(ax, xc - 10, y3 - h/2 - 1, x4, y4 + h/2)
draw_arrow(ax, xc + 10, y3 - h/2 - 1, x5, y4 + h/2)

plt.subplots_adjust(left=0.01, right=0.99, top=0.99, bottom=0.01)
png_filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/generic_workflow_refined.png'
plt.savefig(png_filename, dpi=300, bbox_inches='tight', pad_inches=0.1)
print(f"Done saving {png_filename}")

# -----------------------------------------------------------------------------
# 2. LATEX TIKZ GENERATION
# -----------------------------------------------------------------------------
tex_filename = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/generic_workflow_refined.tex'
tex_content = r"""\documentclass[tikz,border=10pt]{standalone}
\usepackage{helvet}
\renewcommand{\familydefault}{\sfdefault}
\usetikzlibrary{positioning, shapes.multipart, arrows.meta, backgrounds, calc}

\begin{document}
\begin{tikzpicture}[
    node distance=1.5cm and 0.5cm,
    >={Stealth[length=3mm, width=2mm]},
    box/.style={
        rectangle split,
        rectangle split parts=2,
        rectangle split part fill={#1, #2},
        draw=#3,
        thick,
        rounded corners=4pt,
        align=left,
        text width=8.5cm,
        font=\small\linespread{1.1}\selectfont
    },
    titlebox/.style={
        rectangle,
        fill=#1,
        draw=#2,
        thick,
        rounded corners=4pt,
        align=center,
        text width=3.5cm,
        font=\bfseries\color{white},
        inner sep=8pt
    },
    arrow/.style={
        ->,
        thick,
        draw=black!70
    }
]

% Colors
\definecolor{blue_border}{HTML}{2B6CB0}
\definecolor{blue_header}{HTML}{3182CE}
\definecolor{blue_body}{HTML}{EBF8FF}

\definecolor{green_border}{HTML}{276749}
\definecolor{green_header}{HTML}{38A169}
\definecolor{green_body}{HTML}{F0FFF4}

\definecolor{purple_border}{HTML}{6B46C1}
\definecolor{purple_header}{HTML}{805AD5}
\definecolor{purple_body}{HTML}{FAF5FF}

\definecolor{slate_border}{HTML}{4A5568}
\definecolor{slate_header}{HTML}{718096}
\definecolor{slate_body}{HTML}{F7FAFC}

\definecolor{red_border}{HTML}{C53030}
\definecolor{red_header}{HTML}{E53E3E}
\definecolor{red_body}{HTML}{FFF5F5}

\definecolor{orange_border}{HTML}{C05621}
\definecolor{orange_header}{HTML}{DD6B20}
\definecolor{orange_body}{HTML}{FFFAF0}

% Title
\node[font=\Large\bfseries\color{black!80}, align=center, yshift=1cm] at (0, 1.5) {Generic CArt-OnePiece Workflow};

% Box 1
\node[box={blue_header}{blue_body}{blue_border}] (B1) at (0,0) {
    \parbox{\linewidth}{\centering \textbf{\color{white}Simulation Environment}\\[-0.5ex]\textbf{\color{white}Configuration}}
    \nodepart{two}
    \vspace{-1ex}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Scenario definition and CARLA server execution
        \item Ego-vehicle spawn and dynamic actor configuration
        \item Definition of the ADV sensor suite
    \end{itemize}
    \vspace{-2ex}
};

% Box 2
\node[box={green_header}{green_body}{green_border}, below=of B1] (B2) {
    \parbox{\linewidth}{\centering \textbf{\color{white}Sensor Data}\\[-0.5ex]\textbf{\color{white}Acquisition}}
    \nodepart{two}
    \vspace{-1ex}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Collection of active sensor streams
        \item Temporal synchronization and buffering
        \item Data preparation for task-based execution
    \end{itemize}
    \vspace{-2ex}
};

% Box 3
\node[box={purple_header}{purple_body}{purple_border}, below=of B2, text width=10cm] (B3) {
    \parbox{\linewidth}{\centering \textbf{\color{white}ADV Workload and}\\[-0.5ex]\textbf{\color{white}Task Execution}}
    \nodepart{two}
    \vspace{-1ex}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Generation of task graphs from sensor data
        \item Task submission for CPU/GPU heterogeneous scheduling
        \item Execution of modular perception, fusion, and control tasks
    \end{itemize}
    \vspace{-2ex}
};

% Side Box 1 (Left)
\node[titlebox={slate_header}{slate_border}, left=of B3, xshift=-0.5cm] (S1) {User-Defined\\[-0.5ex]Scheduler};

% Side Box 2 (Right)
\node[titlebox={red_header}{red_border}, right=of B3, xshift=0.5cm] (S2) {User-Defined\\[-0.5ex]Processing Module};

% Box 4
\node[box={slate_header}{slate_body}{slate_border}, below left=of B3, text width=7cm, xshift=1.5cm] (B4) {
    \parbox{\linewidth}{\centering \textbf{\color{white}Performance and}\\[-0.5ex]\textbf{\color{white}Analysis Outputs}}
    \nodepart{two}
    \vspace{-1ex}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Runtime latency profiling and trace generation
        \item Resource utilization and workload characterization
        \item Comparative evaluation of scheduling policies
    \end{itemize}
    \vspace{-2ex}
};

% Box 5
\node[box={orange_header}{orange_body}{orange_border}, below right=of B3, text width=7cm, xshift=-1.5cm] (B5) {
    \parbox{\linewidth}{\centering \textbf{\color{white}Synthetic Dataset}\\[-0.5ex]\textbf{\color{white}Generation}}
    \nodepart{two}
    \vspace{-1ex}
    \begin{itemize}\setlength\itemsep{-0.2em}
        \item Synchronized export of sensor data and ground truth
        \item Generation of reproducible annotated ADV datasets
        \item Artifacts for offline model training and validation
    \end{itemize}
    \vspace{-2ex}
};

% Arrows
\draw[arrow] (B1.south) -- (B2.north);
\draw[arrow] (B2.south) -- (B3.north);

\draw[arrow] (S1.east) -- (B3.west);
\draw[arrow] (S2.west) -- (B3.east);

\draw[arrow] ([xshift=-1.5cm]B3.south) -- (B4.north);
\draw[arrow] ([xshift=1.5cm]B3.south) -- (B5.north);

\end{tikzpicture}
\end{document}
"""
with open(tex_filename, 'w') as f:
    f.write(tex_content)
print(f"Done saving {tex_filename}")
