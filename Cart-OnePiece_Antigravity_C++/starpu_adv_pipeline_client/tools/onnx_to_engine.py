import tensorrt as trt
import os
import argparse

def build_engine(onnx_path, engine_path, use_fp16=False):
    logger = trt.Logger(trt.Logger.WARNING)
    builder = trt.Builder(logger)
    network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
    parser = trt.OnnxParser(network, logger)
    
    with open(onnx_path, 'rb') as model:
        if not parser.parse(model.read()):
            for error in range(parser.num_errors):
                print(parser.get_error(error))
            return False
            
    config = builder.create_builder_config()
    
    # Add optimization profile for dynamic shapes or static
    profile = builder.create_optimization_profile()
    # Assuming input name is 'input' and layout is NCHW (1, 3, H, W)
    # Min, Opt, Max
    profile.set_shape("input", (1, 3, 256, 512), (1, 3, 256, 512), (1, 3, 256, 512))
    config.add_optimization_profile(profile)
    
    if builder.platform_has_tf32:
        config.set_flag(trt.BuilderFlag.TF32)
        
    if use_fp16 and builder.platform_has_fast_fp16:
        config.set_flag(trt.BuilderFlag.FP16)
        print("Enabled FP16 precision")
        
    serialized_engine = builder.build_serialized_network(network, config)
    if serialized_engine is None:
        print("ERROR: Failed to build serialized network")
        return False
        
    with open(engine_path, 'wb') as f:
        f.write(serialized_engine)
    print(f"Engine saved to {engine_path}")
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--onnx", default="models/dummy.onnx", help="Input ONNX file")
    parser.add_argument("--engine", default="models/dummy.engine", help="Output TRT engine file")
    parser.add_argument("--fp16", action="store_true", help="Enable FP16 precision")
    args = parser.parse_args()
    
    build_engine(args.onnx, args.engine, args.fp16)
