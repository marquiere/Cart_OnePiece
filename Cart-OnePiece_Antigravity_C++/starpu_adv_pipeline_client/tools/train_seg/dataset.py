import os
import glob
import torch
from torch.utils.data import Dataset
from PIL import Image
import numpy as np
import random

class CarlaSegDataset(Dataset):
    def __init__(self, data_dir, out_h=256, out_w=512, augment=False):
        self.data_dir = data_dir
        self.out_h = out_h
        self.out_w = out_w
        self.augment = augment
        
        self.rgb_files = sorted(glob.glob(os.path.join(data_dir, "rgb", "*.png")))
        self.gt_files = [f.replace("rgb/rgb_", "gt_raw/gt_").replace(".png", ".bin") for f in self.rgb_files]
        
        valid_rgb, valid_gt = [], []
        for rgb, gt in zip(self.rgb_files, self.gt_files):
            if os.path.exists(gt):
                valid_rgb.append(rgb)
                valid_gt.append(gt)
                
        self.rgb_files = valid_rgb
        self.gt_files = valid_gt
        print(f"Found {len(self.rgb_files)} valid frames in {data_dir}")

    def __len__(self):
        return len(self.rgb_files)

    def __getitem__(self, idx):
        rgb_path = self.rgb_files[idx]
        img = Image.open(rgb_path).convert("RGB")
        w, h = img.size
        
        gt_path = self.gt_files[idx]
        gt_raw = np.fromfile(gt_path, dtype=np.uint8)
        gt_img = Image.fromarray(gt_raw.reshape((h, w)), mode="L")
        
        # Resize
        img = img.resize((self.out_w, self.out_h), Image.BILINEAR)
        gt_img = gt_img.resize((self.out_w, self.out_h), Image.NEAREST)
        
        # Augmentation
        if self.augment and random.random() > 0.5:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
            gt_img = gt_img.transpose(Image.FLIP_LEFT_RIGHT)
            
        # Convert RGB to normalized tensor
        img_np = np.array(img, dtype=np.float32) / 255.0
        # ImageNet mean & std
        mean = np.array([0.485, 0.456, 0.406], dtype=np.float32)
        std = np.array([0.229, 0.224, 0.225], dtype=np.float32)
        img_np = (img_np - mean) / std
        
        # HWC to CHW
        img_np = np.transpose(img_np, (2, 0, 1))
        img_t = torch.from_numpy(img_np)
        
        # Convert GT
        gt_t = torch.from_numpy(np.array(gt_img, dtype=np.int64))
        
        return img_t, gt_t

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        ds = CarlaSegDataset(sys.argv[1], 256, 512, augment=True)
        img, gt = ds[0]
        print(f"Img shape: {img.shape}, dtype: {img.dtype}, min: {img.min()}, max: {img.max()}")
        print(f"GT shape: {gt.shape}, dtype: {gt.dtype}, min: {gt.min()}, max: {gt.max()}")
