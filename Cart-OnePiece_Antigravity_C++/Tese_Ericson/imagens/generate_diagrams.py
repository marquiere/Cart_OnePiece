import matplotlib.pyplot as plt
import matplotlib.patches as patches
import os

# Create output directory
os.makedirs('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams', exist_ok=True)

# First approach: Very close to Felipe Lara's example (Block architecture)
def create_block_diagram():
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Set axis limits and hide axes
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis('off')
    
    # Colors
    box_ec = '#1B508A'
    box_fc = 'white'
    text_color = 'black'
    num_bg = '#1B508A'
    num_txt = 'white'

    # Step 1
    ax.add_patch(patches.Rectangle((0, 80), 5, 18, facecolor=num_bg, edgecolor='none', zorder=2))
    ax.text(2.5, 89, '1', color=num_txt, fontsize=16, fontweight='bold', ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((6, 80), 94, 18, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=1))
    
    ax.add_patch(patches.FancyBboxPatch((8, 83), 30, 12, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=2))
    ax.text(23, 89, 'Literature Review\n& Technological Definition', color=text_color, fontsize=11, ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((42, 87), 30, 6, boxstyle="round,pad=0.2", ec='gray', fc=box_fc, zorder=2))
    ax.text(57, 90, 'Phase 1: Planning', color=text_color, fontsize=11, ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((42, 81), 30, 5, boxstyle="square,pad=0", ec='gray', fc=box_fc, zorder=2))
    ax.text(57, 83.5, 'State of the Art', color=text_color, fontsize=11, ha='center', va='center', zorder=3)

    ax.add_patch(patches.FancyBboxPatch((76, 83), 20, 12, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=2))
    ax.text(86, 89, 'CARLA Simulator\nStarPU Runtime', color=text_color, fontsize=11, ha='center', va='center', zorder=3)
    
    # Arrow 1->2
    ax.annotate('', xy=(52, 79), xytext=(52, 75), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))

    # Step 2
    ax.add_patch(patches.Rectangle((0, 35), 5, 42, facecolor=num_bg, edgecolor='none', zorder=2))
    ax.text(2.5, 56, '2', color=num_txt, fontsize=16, fontweight='bold', ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((6, 35), 94, 42, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=1))
    ax.text(52, 74, 'Development of the Experimental Test-Bed (Cart-OnePiece)', color=text_color, fontsize=12, ha='center', va='center', zorder=3)
    
    # Sub-boxes in Step 2
    ax.add_patch(patches.Rectangle((8, 38), 24, 32, fill=False, edgecolor=num_bg, linestyle=':', linewidth=2, zorder=2))
    ax.add_patch(patches.FancyBboxPatch((10, 57), 20, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=3))
    ax.text(20, 62, 'Data Generation\n(CARLA)', color=text_color, fontsize=10, ha='center', va='center', zorder=4)
    ax.add_patch(patches.FancyBboxPatch((10, 42), 20, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=3))
    ax.text(20, 47, 'Sensor Simulation\n& Extraction', color=text_color, fontsize=10, ha='center', va='center', zorder=4)

    # ... inner arrow
    ax.annotate('', xy=(34, 54), xytext=(32, 54), arrowprops=dict(facecolor='#8FB8E3', edgecolor='none', width=2, headwidth=10, shrink=0), zorder=3)

    ax.add_patch(patches.Rectangle((34, 38), 20, 32, fill=False, edgecolor=num_bg, linestyle=':', linewidth=2, zorder=2))
    ax.add_patch(patches.FancyBboxPatch((35, 49), 18, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=3))
    ax.text(44, 54, 'Data\nPreprocessing', color=text_color, fontsize=10, ha='center', va='center', zorder=4)

    # ... inner arrow
    ax.annotate('', xy=(56, 54), xytext=(54, 54), arrowprops=dict(facecolor='#8FB8E3', edgecolor='none', width=2, headwidth=10, shrink=0), zorder=3)

    ax.add_patch(patches.Rectangle((56, 38), 20, 32, fill=False, edgecolor=num_bg, linestyle=':', linewidth=2, zorder=2))
    ax.add_patch(patches.FancyBboxPatch((57, 57), 18, 8, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=3))
    ax.text(66, 61, 'DAG Task Creation', color=text_color, fontsize=10, ha='center', va='center', zorder=4)
    ax.add_patch(patches.FancyBboxPatch((57, 43), 18, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=3))
    ax.text(66, 48, 'Heterogeneous\nScheduling', color=text_color, fontsize=10, ha='center', va='center', zorder=4)

    # ... inner arrow
    ax.annotate('', xy=(78, 54), xytext=(76, 54), arrowprops=dict(facecolor='#8FB8E3', edgecolor='none', width=2, headwidth=10, shrink=0), zorder=3)
    
    ax.add_patch(patches.Rectangle((78, 38), 20, 32, fill=False, edgecolor=num_bg, linestyle=':', linewidth=2, zorder=2))
    ax.add_patch(patches.FancyBboxPatch((79, 49), 18, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=3))
    ax.text(88, 54, 'TensorRT\nInference', color=text_color, fontsize=10, ha='center', va='center', zorder=4)
    
    # Arrows 2->3
    ax.annotate('', xy=(20, 31), xytext=(20, 35), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))
    ax.annotate('', xy=(44, 31), xytext=(44, 35), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))
    ax.annotate('', xy=(66, 31), xytext=(66, 35), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))
    ax.annotate('', xy=(88, 31), xytext=(88, 35), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))

    # Step 3
    ax.add_patch(patches.Rectangle((0, 24), 5, 7, facecolor=num_bg, edgecolor='none', zorder=2))
    ax.text(2.5, 27.5, '3', color=num_txt, fontsize=16, fontweight='bold', ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((6, 24), 94, 7, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=1))
    ax.text(52, 27.5, 'Workload Characterization (End-to-End Latency, Traces, APEX Metrics)', color=text_color, fontsize=11, ha='center', va='center', zorder=3)

    # Arrow 3->5
    ax.annotate('', xy=(20, 18), xytext=(20, 24), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))

    # Step 5
    ax.add_patch(patches.Rectangle((0, 0), 5, 18, facecolor=num_bg, edgecolor='none', zorder=2))
    ax.text(2.5, 9, '4', color=num_txt, fontsize=16, fontweight='bold', ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((6, 0), 38, 18, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=1))
    ax.text(25, 9, 'Experimental Evaluation\n& Policy Comparison', color=text_color, fontsize=11, ha='center', va='center', zorder=3)

    # Arrow 4->5
    ax.annotate('', xy=(44, 9), xytext=(48, 9), arrowprops=dict(facecolor=num_bg, edgecolor='none', width=6, headwidth=15, shrink=0))

    # Step 4 (Experiment Box)
    ax.add_patch(patches.Rectangle((48, 0), 5, 18, facecolor=num_bg, edgecolor='none', zorder=2))
    ax.text(50.5, 9, 'Exp', color=num_txt, fontsize=12, fontweight='bold', ha='center', va='center', zorder=3, rotation=90)
    
    ax.add_patch(patches.FancyBboxPatch((54, 0), 46, 18, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=1))
    ax.text(77, 15, 'Tested Scheduling Policies', color=text_color, fontsize=11, ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((56, 2), 11, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=2))
    ax.text(61.5, 7, 'Urban\nScenario', color=text_color, fontsize=9, ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((68, 2), 11, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=2))
    ax.text(73.5, 7, 'Eager\nPolicy', color=text_color, fontsize=9, ha='center', va='center', zorder=3)
    
    ax.add_patch(patches.FancyBboxPatch((80, 2), 11, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=2))
    ax.text(85.5, 7, 'Prio\nPolicy', color=text_color, fontsize=9, ha='center', va='center', zorder=3)

    ax.add_patch(patches.FancyBboxPatch((92, 2), 7, 10, boxstyle="round,pad=0.5", ec='gray', fc=box_fc, zorder=2))
    ax.text(95.5, 7, 'DMDA\nPolicy', color=text_color, fontsize=9, ha='center', va='center', zorder=3)
    
    plt.tight_layout()
    plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams/1_felipe_lara_style.png', dpi=300, bbox_inches='tight')
    plt.close()

# Second approach: Vertical Pipeline Flow (Top to Bottom)
def create_vertical_pipeline():
    fig, ax = plt.subplots(figsize=(8, 12))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis('off')
    
    colors = ['#f8b195', '#f67280', '#c06c84', '#6c5b7b']
    
    # Titles and desc
    y_pos = 90
    for i, (title, color, text) in enumerate(zip(
        ['Step 1: Literature\n& Tech Definition', 'Step 2: Experimental\nTest-Bed Dev', 'Step 3: Workload\nCharacterization', 'Step 4: Experimental\nEvaluation & Comparison'],
        colors,
        ['Literature Review\nCARLA & StarPU Platforms\nTest Environment Rules', 
         'C++ Client Simulation/Runtime\nData Processing Configuration\nHeterogeneous Architecture setup',
         'End-to-End Latency Timestamps\nAPEX Memory Metrics\nFxT Traces Gathering',
         'Policy Efficiency Analysis\nValidation against Ground Truth\nComparison across Scheduler algorithms'])):
        
        ax.add_patch(patches.FancyBboxPatch((20, y_pos-12), 60, 16, boxstyle="round,pad=0.5", ec=color, fc='white', lw=3, zorder=2))
        ax.add_patch(patches.Rectangle((18, y_pos-4), 22, 10, facecolor=color, zorder=3))
        ax.text(29, y_pos+1, title, color='white', fontweight='bold', ha='center', va='center', zorder=4, fontsize=9)
        
        ax.text(60, y_pos+1, text, fontsize=10, color='black', ha='center', va='center', zorder=4)
        
        if i < 3:
            ax.annotate('', xy=(50, y_pos-20), xytext=(50, y_pos-14), arrowprops=dict(facecolor=color, edgecolor='none', width=4, headwidth=10, shrink=0))
        
        y_pos -= 24

    plt.tight_layout()
    plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams/2_vertical_pipeline.png', dpi=300, bbox_inches='tight')
    plt.close()

# Third approach: Central Database / Environment model
def create_central_hub_model():
    fig, ax = plt.subplots(figsize=(10, 10))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis('off')
    
    # Central Environment
    ax.add_patch(patches.Circle((50, 50), 20, facecolor='#eef2f3', edgecolor='#8e9eab', lw=4, zorder=1))
    ax.text(50, 53, 'Cart-OnePiece\nFramework', fontsize=14, fontweight='bold', ha='center', va='center', zorder=2)
    ax.text(50, 45, '(CARLA + StarPU)', fontsize=11, color='gray', ha='center', va='center', zorder=2)

    # Peripheral nodes
    nodes = [
        (50, 85, '1. Definition', 'Lit. Review\nTech Stack (C++/Python)'),
        (85, 50, '2. Scene Setup', 'Urban Simulation\nVirtual Sensors'),
        (50, 15, '3. Heterogeneous Execution', 'StarPU Task Scheduling\nTensorRT Inference'),
        (15, 50, '4. Characterization', 'FxT Traces, APEX\nPolicy Evaluation')
    ]
    
    for nx, ny, title, desc in nodes:
        ax.add_patch(patches.FancyBboxPatch((nx-15, ny-8), 30, 16, boxstyle="round,pad=0.3", ec='#2c3e50', fc='white', lw=2, zorder=3))
        ax.text(nx, ny+2, title, fontsize=12, fontweight='bold', color='#c0392b', ha='center', va='center', zorder=4)
        ax.text(nx, ny-3, desc, fontsize=10, ha='center', va='center', zorder=4)
        
        # Connect to center
        if ny == 85: ax.annotate('', xy=(50, 70), xytext=(50, 77), arrowprops=dict(facecolor='black', width=2, headwidth=8))
        if nx == 85: ax.annotate('', xy=(70, 50), xytext=(77, 50), arrowprops=dict(facecolor='black', width=2, headwidth=8))
        if ny == 15: ax.annotate('', xy=(50, 30), xytext=(50, 23), arrowprops=dict(facecolor='black', width=2, headwidth=8))
        if nx == 15: ax.annotate('', xy=(30, 50), xytext=(23, 50), arrowprops=dict(facecolor='black', width=2, headwidth=8))
        
    plt.tight_layout()
    plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams/3_central_hub.png', dpi=300, bbox_inches='tight')
    plt.close()

# Fourth approach: Timeline / Phased approach
def create_timeline():
    fig, ax = plt.subplots(figsize=(14, 6))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis('off')
    
    # Main timeline
    ax.add_patch(patches.Rectangle((10, 48), 80, 4, facecolor='#34495e', zorder=1))
    
    phases = [
        (20, '1: Tech Definition', 'Lit Review, CARLA,\nStarPU setup', '#3498db'),
        (40, '2: Test-Bed Dev', 'C++ Pipeline,\nSensor sync', '#2ecc71'),
        (60, '3: Characterization', 'Heterogeneous task\nscheduling testing', '#f1c40f'),
        (80, '4: Evaluation', 'Metrics extraction,\nPolicy compare', '#e74c3c')
    ]
    
    for px, title, desc, col in phases:
        ax.add_patch(patches.Circle((px, 50), 3, facecolor=col, edgecolor='white', lw=2, zorder=2))
        
        # Alternating text
        if px in [20, 60]:
            y_box = 60
            y_text_title = 68
            y_text_desc = 63
            ax.plot([px, px], [53, y_box], color='gray', linestyle='--')
        else:
            y_box = 20
            y_text_title = 35
            y_text_desc = 30
            ax.plot([px, px], [47, y_box+16], color='gray', linestyle='--')
            
        ax.add_patch(patches.FancyBboxPatch((px-12, y_box), 24, 16, boxstyle="round,pad=0.2", ec=col, fc='white', lw=2, zorder=3))
        ax.text(px, y_text_title, title, fontsize=11, fontweight='bold', color=col, ha='center', va='center', zorder=4)
        ax.text(px, y_text_desc, desc, fontsize=9, ha='center', va='center', zorder=4)

    plt.tight_layout()
    plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams/4_timeline_phases.png', dpi=300, bbox_inches='tight')
    plt.close()

# Fifth approach: Layered Architecture
def create_layered():
    fig, ax = plt.subplots(figsize=(10, 8))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis('off')
    
    layers = [
        (80, '#f9ebea', '#c0392b', 'Step 4: Experimental Evaluation', 'Hypothesis Validation, Performance Comparison, Trace Visualization (StarVZ)'),
        (60, '#ebf5fb', '#2980b9', 'Step 3: Workload Characterization', 'End-to-End Latency Tracking, APEX Memory Profiling, DAG Extraction'),
        (40, '#fef9e7', '#f39c12', 'Step 2: Experimental Test-Bed', 'Task Dependencies, Policy Dispatching, Heterogeneous Architecture (CARLA+StarPU)'),
        (20, '#e8f8f5', '#1abc9c', 'Step 1: Technological Definition', 'Literature Review, Hardware / Software Boundaries')
    ]
    
    for y, fc, ec, title, desc in layers:
        ax.add_patch(patches.FancyBboxPatch((10, y-15), 80, 14, boxstyle="round,pad=0", fc=fc, ec=ec, lw=2, zorder=1))
        ax.text(12, y-6, title, fontsize=13, fontweight='bold', color=ec, ha='left', va='center', zorder=2)
        ax.text(12, y-11, desc, fontsize=10, color='black', ha='left', va='center', zorder=2)
        
        # Connectors
        if y > 20:
            ax.annotate('', xy=(50, y-15), xytext=(50, y-16), arrowprops=dict(facecolor='gray', edgecolor='none', width=2, headwidth=10, shrink=0))
            ax.annotate('', xy=(50, y-16), xytext=(50, y-15), arrowprops=dict(facecolor='gray', edgecolor='none', width=2, headwidth=10, shrink=0))

    plt.tight_layout()
    plt.savefig('/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams/5_layered_architecture.png', dpi=300, bbox_inches='tight')
    plt.close()

if __name__ == '__main__':
    create_block_diagram()
    create_vertical_pipeline()
    create_central_hub_model()
    create_timeline()
    create_layered()
    print("Done generating 5 images in /home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/imagens/method_diagrams")
