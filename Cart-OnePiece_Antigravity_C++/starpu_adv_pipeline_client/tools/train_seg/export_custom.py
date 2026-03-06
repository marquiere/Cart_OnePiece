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

def export_custom_model():
    output_dir = "models"
    os.makedirs(output_dir, exist_ok=True)
    onnx_path = os.path.join(output_dir, "carla_resnet50_best.onnx")
    engine_path = os.path.join(output_dir, "carla_resnet50_best.engine")
    pth_path = os.path.join(output_dir, "carla_resnet50_best.pth")

    print(f"=== Exporting Custom CARLA ResNet50 ===")
    
    # Initialize the blank model with 26 CARLA raw classes
    model = segmentation.fcn_resnet50(num_classes=26)
    
    # Load the trained CARLA weights
    if not os.path.exists(pth_path):
        print(f"Error: {pth_path} not found! Did you run train.py?")
        sys.exit(1)
        
    model.load_state_dict(torch.load(pth_path, map_location='cpu'))
    
    # VITAL BYPASS: Extract static components to bypass dynamic Upsample failure in TensorRT
    class StaticExportWrapper(torch.nn.Module):
        def __init__(self, base_model):
            super().__init__()
            self.backbone = base_model.backbone
            self.classifier = base_model.classifier
            
        def forward(self, x):
            features = self.backbone(x)
            x = features['out'] 
            x = self.classifier(x)
            # STRICTLY STATIC upsample matching inference resolution
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
        opset_version=18,
        do_constant_folding=True,
        input_names=["input_tensor"], 
        output_names=["output_logits"]
    )
    
    print(f"Simplifying ONNX graph...")
    onnx_model = onnx.load(onnx_path)
    model_simp, check = onnxsim.simplify(onnx_model)
    assert check, "Simplified ONNX model could not be validated"
    onnx.save(model_simp, onnx_path)
    
    build_engine(onnx_path, engine_path)

if __name__ == "__main__":
    export_custom_model()
