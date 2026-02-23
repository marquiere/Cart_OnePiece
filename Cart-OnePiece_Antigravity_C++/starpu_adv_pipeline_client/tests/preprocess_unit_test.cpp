#include "preprocess.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

bool float_eq(float a, float b, float epsilon = 1e-4f) {
  return std::fabs(a - b) < epsilon;
}

void test_bgra_to_rgb_no_resize() {
  std::cout << "Running test_bgra_to_rgb_no_resize..." << std::endl;
  // 2x2 image
  int w = 2, h = 2;
  std::vector<uint8_t> bgra = {
      255, 0,   0,   255, // Pixel 0,0: Blue
      0,   255, 0,   255, // Pixel 1,0: Green
      0,   0,   255, 255, // Pixel 0,1: Red
      255, 255, 255, 255  // Pixel 1,1: White
  };

  PreprocessConfig cfg;
  cfg.out_w = 2;
  cfg.out_h = 2;
  cfg.assume_bgra = true;
  cfg.to_rgb = true; // We want RGB output
  cfg.resize = PreprocessConfig::NEAREST;

  // Identity normalization
  cfg.mean[0] = 0.0f;
  cfg.mean[1] = 0.0f;
  cfg.mean[2] = 0.0f;
  cfg.std[0] = 1.0f;
  cfg.std[1] = 1.0f;
  cfg.std[2] = 1.0f;

  std::vector<float> nchw(3 * w * h, 0.0f);
  bool ok = PreprocessBGRAtoNCHW_F32(bgra.data(), w, h, cfg, nchw.data());

  assert(ok && "Preprocessing failed");

  // Output is NCHW, so it's planar: RRRR GGGG BBBB
  size_t stride = w * h;

  // Check Pixel 0,0 (Blue originally, so R=0, G=0, B=1.0)
  assert(float_eq(nchw[0 * stride + 0], 0.0f) && "R channel mismatch at 0,0");
  assert(float_eq(nchw[1 * stride + 0], 0.0f) && "G channel mismatch at 0,0");
  assert(float_eq(nchw[2 * stride + 0], 1.0f) && "B channel mismatch at 0,0");

  // Check Pixel 1,0 (Green originally, so R=0, G=1.0, B=0)
  assert(float_eq(nchw[0 * stride + 1], 0.0f) && "R channel mismatch at 1,0");
  assert(float_eq(nchw[1 * stride + 1], 1.0f) && "G channel mismatch at 1,0");
  assert(float_eq(nchw[2 * stride + 1], 0.0f) && "B channel mismatch at 1,0");

  // Check Pixel 0,1 (Red originally, so R=1.0, G=0, B=0)
  assert(float_eq(nchw[0 * stride + 2], 1.0f) && "R channel mismatch at 0,1");
  assert(float_eq(nchw[1 * stride + 2], 0.0f) && "G channel mismatch at 0,1");
  assert(float_eq(nchw[2 * stride + 2], 0.0f) && "B channel mismatch at 0,1");
}

void test_normalization_math() {
  std::cout << "Running test_normalization_math..." << std::endl;
  int w = 1, h = 1;
  // Single pixel, mid-gray
  std::vector<uint8_t> bgra = {128, 128, 128, 255};

  PreprocessConfig cfg;
  cfg.out_w = 1;
  cfg.out_h = 1;
  cfg.assume_bgra = true;
  cfg.to_rgb = true;
  cfg.resize = PreprocessConfig::NEAREST;

  // Custom mean/std
  cfg.mean[0] = 0.5f;
  cfg.mean[1] = 0.2f;
  cfg.mean[2] = 0.8f;
  cfg.std[0] = 0.1f;
  cfg.std[1] = 0.2f;
  cfg.std[2] = 0.5f;

  std::vector<float> nchw(3 * w * h, 0.0f);
  bool ok = PreprocessBGRAtoNCHW_F32(bgra.data(), w, h, cfg, nchw.data());

  assert(ok && "Preprocessing failed");

  float p_val = 128.0f / 255.0f; // roughly 0.50196

  float expected_r = (p_val - 0.5f) / 0.1f;
  float expected_g = (p_val - 0.2f) / 0.2f;
  float expected_b = (p_val - 0.8f) / 0.5f;

  assert(float_eq(nchw[0], expected_r) && "Math mismatch on channel 0");
  assert(float_eq(nchw[1], expected_g) && "Math mismatch on channel 1");
  assert(float_eq(nchw[2], expected_b) && "Math mismatch on channel 2");
}

void test_bgr_output() {
  std::cout << "Running test_bgr_output..." << std::endl;
  int w = 1, h = 1;
  std::vector<uint8_t> bgra = {255, 0, 0, 255}; // Blue

  PreprocessConfig cfg;
  cfg.out_w = 1;
  cfg.out_h = 1;
  cfg.assume_bgra = true;
  cfg.to_rgb = false; // We want BGR output this time
  cfg.resize = PreprocessConfig::NEAREST;
  cfg.mean[0] = 0.0f;
  cfg.mean[1] = 0.0f;
  cfg.mean[2] = 0.0f;
  cfg.std[0] = 1.0f;
  cfg.std[1] = 1.0f;
  cfg.std[2] = 1.0f;

  std::vector<float> nchw(3 * w * h, 0.0f);
  bool ok = PreprocessBGRAtoNCHW_F32(bgra.data(), w, h, cfg, nchw.data());
  assert(ok && "Preprocessing failed");

  // Expected Output should be B G R, so Channel 0 should be Blue (1.0)
  assert(float_eq(nchw[0], 1.0f) && "Channel 0 should be Blue");
  assert(float_eq(nchw[1], 0.0f) && "Channel 1 should be Green");
  assert(float_eq(nchw[2], 0.0f) && "Channel 2 should be Red");
}

int main() {
  std::cout << "--- Starting Preprocess Unit Tests ---" << std::endl;

  test_bgra_to_rgb_no_resize();
  test_normalization_math();
  test_bgr_output();

  std::cout << "--- All Tests Passed Successfully! ---" << std::endl;
  return 0;
}
