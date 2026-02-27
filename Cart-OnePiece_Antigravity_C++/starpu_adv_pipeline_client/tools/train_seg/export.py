import torch
import os
import onnx
from model import SimpleSeg

def export():
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model_path = "models/carla_seg_best.pth"
    onnx_path = "models/carla_seg.onnx"
    
    model = SimpleSeg(in_channels=3, out_classes=26).to(device)
    model.load_state_dict(torch.load(model_path, map_location=device))
    model.eval()
    
    dummy_input = torch.randn(1, 3, 256, 512, device=device)
    
    print(f"Exporting ONNX to {onnx_path}...")
    torch.onnx.export(
        model, 
        dummy_input, 
        onnx_path, 
        export_params=True,
        opset_version=11, 
        do_constant_folding=True,
        input_names=['input'], 
        output_names=['output']
    )
    
    # Verify
    onnx_model = onnx.load(onnx_path)
    onnx.checker.check_model(onnx_model)
    print("ONNX model verification passed.")
    print(f"Saved: {onnx_path} ({os.path.getsize(onnx_path)} bytes)")

if __name__ == "__main__":
    export()
