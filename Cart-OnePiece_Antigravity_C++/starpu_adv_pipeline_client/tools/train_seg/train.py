import os
import glob
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, random_split
from dataset import CarlaSegDataset
import torchvision.models.segmentation as segmentation

def train():
    data_dir = max(glob.glob("runs/*/sanity_dataset"), key=os.path.getmtime)
    print("Using dataset:", data_dir)
    
    full_dataset = CarlaSegDataset(data_dir, out_h=256, out_w=512, augment=True)
    
    train_size = int(0.8 * len(full_dataset))
    val_size = len(full_dataset) - train_size
    train_dataset, val_dataset = random_split(full_dataset, [train_size, val_size])
    
    train_loader = DataLoader(train_dataset, batch_size=4, shuffle=True, num_workers=2)
    val_loader = DataLoader(val_dataset, batch_size=4, shuffle=False)
    
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print("Device:", device)
    
    # Initialize blank un-trained fcn_resnet50 tailored to 26 CARLA raw classes
    model = segmentation.fcn_resnet50(num_classes=26).to(device)
    
    criterion = nn.CrossEntropyLoss(ignore_index=0) # 0 is unlabeled in CARLA mostly
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    
    num_epochs = 25
    best_loss = float('inf')
    
    os.makedirs("models", exist_ok=True)
    os.makedirs("tools/train_seg/debug_pred", exist_ok=True)
    
    print("--- STARTING TRAINING ---")
    for epoch in range(num_epochs):
        model.train()
        train_loss = 0.0
        
        for imgs, masks in train_loader:
            imgs = imgs.to(device)
            masks = masks.to(device)
            
            optimizer.zero_grad()
            features = model(imgs) # TorchVision returns OrderedDict
            outputs = features['out'] # [B, 26, H, W]
            
            loss = criterion(outputs, masks)
            loss.backward()
            optimizer.step()
            
            train_loss += loss.item() * imgs.size(0)
            
        train_loss /= len(train_loader.dataset)
        
        # Validation
        model.eval()
        val_loss = 0.0
        correct = 0
        total = 0
        
        with torch.no_grad():
            for imgs, masks in val_loader:
                imgs = imgs.to(device)
                masks = masks.to(device)
                
                features = model(imgs)
                outputs = features['out'] # logits
                
                loss = criterion(outputs, masks)
                val_loss += loss.item() * imgs.size(0)
                
                preds = torch.argmax(outputs, dim=1) # [B, H, W]
                correct += (preds == masks).sum().item()
                total += masks.numel()
        
        val_loss /= len(val_loader.dataset)
        pixel_acc = correct / total
        
        print(f"Epoch {epoch+1}/{num_epochs} - Train Loss: {train_loss:.4f}, Val Loss: {val_loss:.4f}, Val Acc: {pixel_acc:.4f}")
        
        if val_loss < best_loss:
            best_loss = val_loss
            # Save the ResNet50 state dict specifically
            torch.save(model.state_dict(), "models/carla_resnet50_best.pth")
            print("  (*) Saved best model checkpoint.")

if __name__ == "__main__":
    train()
