import os
import glob
import shutil
import numpy as np

# Find the latest dataset_sanity run
latest_run = max(glob.glob("runs/*/sanity_dataset"), key=os.path.getmtime)
print(f"Using run dir: {latest_run}")

# Create debug_seg dir
debug_dir = os.path.join(latest_run, "debug_seg")
os.makedirs(debug_dir, exist_ok=True)

# Find first frame ID
gt_files = sorted(glob.glob(os.path.join(latest_run, "gt_raw", "*.bin")))
if not gt_files:
    print("No GT files found!")
    exit(1)

first_frame = os.path.basename(gt_files[0]).split('_')[1].split('.')[0]

# Copy 5 PNG samples 
img_types = ['rgb/rgb', 'gt_color/gt', 'pred_color/pred', 'overlay/overlay']
for itype in img_types:
    src = os.path.join(latest_run, f"{itype}_{first_frame}.png")
    dst = os.path.join(debug_dir, f"{os.path.basename(itype.split('/')[0])}.png")
    if os.path.exists(src):
        shutil.copy(src, dst)
        print(f"Copied {dst}")

# Process pred_labels_hist.txt for the specific frame
pred_bin = os.path.join(latest_run, "pred_raw", f"pred_{first_frame}.bin")
if os.path.exists(pred_bin):
    pred = np.fromfile(pred_bin, dtype=np.uint8)
    u, c = np.unique(pred, return_counts=True)
    
    with open(os.path.join(debug_dir, "pred_labels_hist.txt"), "w") as f:
        f.write(f"Frame ID: {first_frame}\n")
        f.write(f"pred_label_min/max: {pred.min()} / {pred.max()}\n")
        f.write(f"unique_count: {len(u)}\n")
        f.write("top-10 classes with percentages:\n")
        
        # Calculate exactly as the prompt requires: "pred_label_min/max, unique_count, top-10 classes with percentages."
        print(f"pred_label_min/max: {pred.min()} / {pred.max()}, unique_count: {len(u)}, top-10 classes:")
        for v, cnt in sorted(zip(u, c), key=lambda x: x[1], reverse=True)[:10]:
            pct = (cnt / len(pred)) * 100
            f.write(f"  Class {v:2d}: {pct:.1f}%\n")
            print(f"  Class {v:2d}: {pct:.1f}%")
        
    print(f"Saved {os.path.join(debug_dir, 'pred_labels_hist.txt')}")
else:
    print("Pred file not found")
