#!/usr/bin/env python3
import os
import sys
import torch
import torch.nn.functional as F

output_dir = "models"
os.makedirs(output_dir, exist_ok=True)
onnx_path = os.path.join(output_dir, "cityscapes_hrnet_direct.onnx")
engine_path = os.path.join(output_dir, "cityscapes_hrnet_direct.engine")

# The Segformer weights exceeded the 2GB Protocol Buffer limit, forcing PyTorch to dump an .onnx.data side file
# which TensorRT 10.x C++ API notoriously fails to locate correctly if paths are not absolute.
# Instead of dealing with Transformer external weights, let's grab a classic PyTorch Hub model guaranteed to be under 2GB.

print("=== Downloading Pre-Trained Cityscapes Model ===")
print("Source: PyTorch Hub (lraspp_mobilenet_v3_large / Native TorchVision)")
import torchvision.models.segmentation as segmentation

try:
    print("Fetching LRASPP MobileNetV3 Large (Cityscapes Native Model)...")
    # Note: `DEFAULT` weights for lraspp are COCO. But there is no simple builtin Cityscapes flag.
    # We will use this model to prove the pipeline works, but it will hallucinate unless we force it to Cityscapes.
     
    # Actually, let's bypass completely and directly use TorchVision's fcn_resnet50 we just trained!
    # Wait, the user wants a pre-trained model they don't have to train themselves.
    
    # HuggingFace is the only way to get a pre-trained cityscapes model easily. 
    # Let's fix the Segformer external data bug for TRT.
    pass
except Exception as e:
    sys.exit(1)

from transformers import SegformerForSemanticSegmentation
model = SegformerForSemanticSegmentation.from_pretrained("nvidia/segformer-b0-finetuned-cityscapes-512-1024")

class SegformerWrapper(torch.nn.Module):
    def __init__(self, base_model):
        super().__init__()
        self.model = base_model
        
    def forward(self, x):
        outputs = self.model(x)
        logits = outputs.logits
        import torch.nn.functional as F
        upsampled = F.interpolate(logits, size=(256, 512), mode="bilinear", align_corners=False)
        # Apply argmax so C++ pipeline receives a direct [B, 1, 256, 512] integer map directly
        preds = torch.argmax(upsampled, dim=1, keepdim=True).to(torch.int32)
        return preds

wrapped_model = SegformerWrapper(model).eval().cuda()
dummy_input = torch.randn(1, 3, 256, 512).cuda()

print("Exporting to ONNX...")

# Since Segformer B0 is only ~14MB, we shouldn't trigger the 2GB limit. 
# The issue might be opset 18 dynamic features triggering side-blobs in newer TRT.
# We will drop opset down to 13 (a very stable ONNX version).

torch.onnx.export(
    wrapped_model, 
    dummy_input, 
    onnx_path,
    export_params=True,
    opset_version=13,
    do_constant_folding=True,
    input_names=["input_tensor"], 
    output_names=["output_logits"]
)

import tensorrt as trt

print(f"Building TensorRT FP16 Engine from {onnx_path}...")
TRT_LOGGER = trt.Logger(trt.Logger.WARNING)
builder = trt.Builder(TRT_LOGGER)
network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
config = builder.create_builder_config()
parser = trt.OnnxParser(network, TRT_LOGGER)

if builder.platform_has_fast_fp16:
    config.set_flag(trt.BuilderFlag.FP16)
    print("FP16 Enabled")

config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 4 * 1024 * 1024 * 1024)

# TRT requires absolute paths to locate `.onnx.data` files internally
abs_onnx_path = os.path.abspath(onnx_path)

with open(abs_onnx_path, 'rb') as fp:
    if not parser.parse(fp.read(), path=abs_onnx_path):  # Provide path so TRT C-API finds external data
        print ('ERROR: Failed to parse the ONNX file.')
        for error in range(parser.num_errors):
            print (parser.get_error(error))
        sys.exit(1)

engineString = builder.build_serialized_network(network, config)
if engineString == None:
    print("Failed building engine")
    sys.exit(1)

with open(engine_path, "wb") as f:
    f.write(engineString)
print(f"Engine saved to: {engine_path}\n")

