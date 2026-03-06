#!/usr/bin/env python3
import os
import sys
import torch
import torchvision.models.segmentation as segmentation
import tensorrt as trt
import onnx
import onnxsim

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

def build_engine(onnx_file_path, engine_file_path):
    print(f"Building TensorRT FP16 Engine from {onnx_file_path}...")
    builder = trt.Builder(TRT_LOGGER)
    network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
    config = builder.create_builder_config()
    parser = trt.OnnxParser(network, TRT_LOGGER)

    if builder.platform_has_fast_fp16:
        config.set_flag(trt.BuilderFlag.FP16)
        print("FP16 Enabled")

    config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 4 * 1024 * 1024 * 1024)

    with open(onnx_file_path, 'rb') as model:
        if not parser.parse(model.read()):
            print ('ERROR: Failed to parse the ONNX file.')
            for error in range(parser.num_errors):
                print (parser.get_error(error))
            return None

    engineString = builder.build_serialized_network(network, config)
    if engineString == None:
        print("Failed building engine")
        return None

    with open(engine_file_path, "wb") as f:
        f.write(engineString)
    print(f"Engine saved to: {engine_file_path}\n")

def export_model(model_name, output_dir="models"):
    os.makedirs(output_dir, exist_ok=True)
    onnx_path = os.path.join(output_dir, f"{model_name}.onnx")
    engine_path = os.path.join(output_dir, f"{model_name}.engine")

    print(f"=== Exporting {model_name} ===")
    
    if model_name == "deeplabv3_mobilenet":
        model = segmentation.deeplabv3_mobilenet_v3_large(pretrained=True)
    elif model_name == "fcn_resnet50":
        model = segmentation.fcn_resnet50(pretrained=True)
    elif model_name == "deeplabv3_resnet50":
        model = segmentation.deeplabv3_resnet50(pretrained=True)
    elif model_name == "fcn_resnet101":
        model = segmentation.fcn_resnet101(pretrained=True)
    else:
        print(f"Unknown model: {model_name}")
        sys.exit(1)

    # VITAL BYPASS: Torchvision's native `.forward()` method uses a Dictionary return and dynamic 
    # spatial resizing which fundamentally breaks the ONNX C-API adapter in PyTorch 2.1.
    # We must explicitly trace the backbone and classifier nodes directly, avoiding the dynamic upsample.
    class StaticExportWrapper(torch.nn.Module):
        def __init__(self, base_model):
            super().__init__()
            self.backbone = base_model.backbone
            self.classifier = base_model.classifier
            
        def forward(self, x):
            # 1. Run backbone explicitly (extracts physical features)
            features = self.backbone(x)
            # Torchvision backbones return a dict, usually we want 'out'
            x = features['out'] 
            
            # 2. Run classifier explicitly
            x = self.classifier(x)
            
            # 3. Perform a STRICTLY STATIC upsample to match the input resolution (512x256)
            # This avoids dynamic "axes" tracing completely.
            x = torch.nn.functional.interpolate(x, size=(256, 512), mode='bilinear', align_corners=False)
            return x

    wrapped_model = StaticExportWrapper(model).eval().cuda()
    dummy_input = torch.randn(1, 3, 256, 512).cuda()

    print(f"Exporting to ONNX: {onnx_path}")
    torch.onnx.export(
        wrapped_model, 
        dummy_input, 
        onnx_path,
        export_params=True,
        opset_version=18, # Use modern opset to match standard operators
        do_constant_folding=True,
        input_names=["input_tensor"], 
        output_names=["output_logits"]
    )
    
    print(f"Simplifying ONNX graph to resolve TRT static weight errors...")
    onnx_model = onnx.load(onnx_path)
    model_simp, check = onnxsim.simplify(onnx_model)
    assert check, "Simplified ONNX model could not be validated"
    onnx.save(model_simp, onnx_path)
    
    build_engine(onnx_path, engine_path)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--models", nargs="+", default=["deeplabv3_mobilenet", "fcn_resnet50"], help="Models to generate")
    args = parser.parse_args()

    print("Generating Real TorchVision Models for Pipeline...")
    for m in args.models:
        export_model(m)
    print("Done! You can now use these engines in the pipeline.")
