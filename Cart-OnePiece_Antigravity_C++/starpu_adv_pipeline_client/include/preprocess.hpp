#pragma once

#include <cstdint>
#include <vector>

struct PreprocessConfig {
  int out_w;
  int out_h;
  bool assume_bgra;
  bool to_rgb;
  float mean[3];
  float std[3];
  enum ResizeMode { NEAREST, BILINEAR } resize;
};

// Implements CPU-side preprocessing of raw BGRA sensors.
// Converts to float32, re-orders channels, resizes, normalizes, and forces NCHW
// memory layout. Note: output buffer `out_nchw` MUST be pre-allocated to size
// (3 * cfg.out_w * cfg.out_h).
bool PreprocessBGRAtoNCHW_F32(const uint8_t *in_bgra, int in_w, int in_h,
                              const PreprocessConfig &cfg, float *out_nchw);
