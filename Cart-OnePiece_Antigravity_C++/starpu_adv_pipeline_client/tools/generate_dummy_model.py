import torch
import torch.nn as nn
import onnx
import os
import io

class DummyModel(nn.Module):
    def __init__(self):
        super(DummyModel, self).__init__()
        self.conv = nn.Conv2d(3, 19, kernel_size=3, padding=1)
        self.relu = nn.ReLU()

    def forward(self, x):
        return self.relu(self.conv(x))

def generate_dummy_onnx(path):
    model = DummyModel().eval()
    dummy_input = torch.randn(1, 3, 256, 512)
    
    # Export to memory buffer first
    f = io.BytesIO()
    torch.onnx.export(model, dummy_input, f, 
                      input_names=['input'], 
                      output_names=['output'],
                      export_params=True,
                      opset_version=11, # Use a safer legacy opset
                      do_constant_folding=True,
                      dynamic_axes={'input': {2: 'height', 3: 'width'},
                                    'output': {2: 'height', 3: 'width'}})
    
    # Load from buffer and save to file manually via onnx library to ensure single file
    onnx_model = onnx.load_model(io.BytesIO(f.getvalue()))
    onnx.save_model(onnx_model, path)
    
    print(f"Dummy ONNX model saved to {path} (Size: {os.path.getsize(path)} bytes)")

if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    generate_dummy_onnx("models/dummy.onnx")
