import glob
import numpy as np
import os

run_dir = "/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/starpu_adv_pipeline_client/runs/20260224_204042/sanity_dataset"

gt_files = sorted(glob.glob(f"{run_dir}/gt_raw/*.bin"))
pred_files = sorted(glob.glob(f"{run_dir}/pred_raw/*.bin"))

for gt_f, pred_f in zip(gt_files, pred_files):
    gt = np.fromfile(gt_f, dtype=np.uint8)
    pred = np.fromfile(pred_f, dtype=np.uint8)
    
    print(f"=== Frame {os.path.basename(gt_f)} ===")
    print(f"GT min: {gt.min()}, max: {gt.max()}")
    g_u, g_c = np.unique(gt, return_counts=True)
    print("GT Top 10:")
    for v, c in sorted(zip(g_u, g_c), key=lambda x: x[1], reverse=True)[:10]:
        print(f"  Class {v:2d}: {c:6d} ({(c/len(gt)*100):.1f}%)")

    print(f"Pred min: {pred.min()}, max: {pred.max()}")
    p_u, p_c = np.unique(pred, return_counts=True)
    print("Pred Top 10:")
    for v, c in sorted(zip(p_u, p_c), key=lambda x: x[1], reverse=True)[:10]:
        print(f"  Class {v:2d}: {c:6d} ({(c/len(pred)*100):.1f}%)")
    break
