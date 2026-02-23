#include "preprocess.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

static void GetSourceIndices(int w_out, int h_out, int w_in, int h_in,
                             PreprocessConfig::ResizeMode mode, int out_x,
                             int out_y, float &src_x, float &src_y) {
  if (mode == PreprocessConfig::NEAREST) {
    src_x =
        (static_cast<float>(out_x) + 0.5f) * (static_cast<float>(w_in) / w_out);
    src_y =
        (static_cast<float>(out_y) + 0.5f) * (static_cast<float>(h_in) / h_out);
  } else { // BILINEAR
    src_x = (static_cast<float>(out_x) + 0.5f) *
                (static_cast<float>(w_in) / w_out) -
            0.5f;
    src_y = (static_cast<float>(out_y) + 0.5f) *
                (static_cast<float>(h_in) / h_out) -
            0.5f;
  }
}

static void BilinearInterpolate(const uint8_t *src, int w_in, int h_in,
                                float src_x, float src_y, int c_offset,
                                float &val) {
  int x1 = std::max(0, static_cast<int>(std::floor(src_x)));
  int y1 = std::max(0, static_cast<int>(std::floor(src_y)));
  int x2 = std::min(w_in - 1, x1 + 1);
  int y2 = std::min(h_in - 1, y1 + 1);

  float dx = src_x - static_cast<float>(x1);
  float dy = src_y - static_cast<float>(y1);

  float p11 = src[(y1 * w_in + x1) * 4 + c_offset];
  float p21 = src[(y1 * w_in + x2) * 4 + c_offset];
  float p12 = src[(y2 * w_in + x1) * 4 + c_offset];
  float p22 = src[(y2 * w_in + x2) * 4 + c_offset];

  val = (1.0f - dx) * (1.0f - dy) * p11 + dx * (1.0f - dy) * p21 +
        (1.0f - dx) * dy * p12 + dx * dy * p22;
}

bool PreprocessBGRAtoNCHW_F32(const uint8_t *in_bgra, int in_w, int in_h,
                              const PreprocessConfig &cfg, float *out_nchw) {
  if (!in_bgra || !out_nchw) {
    std::cerr << "Error: Null pointer passed to PreprocessBGRAtoNCHW_F32"
              << std::endl;
    return false;
  }
  if (in_w <= 0 || in_h <= 0 || cfg.out_w <= 0 || cfg.out_h <= 0) {
    std::cerr << "Error: Invalid dimensions in PreprocessBGRAtoNCHW_F32"
              << std::endl;
    return false;
  }
  if (cfg.std[0] == 0.0f || cfg.std[1] == 0.0f || cfg.std[2] == 0.0f) {
    std::cerr << "Error: Standard deviation cannot be zero" << std::endl;
    return false;
  }

  // Determine channel mapping based on assume_bgra and to_rgb flags.
  // The CARLA buffer layout is B, G, R, A.
  // If we assume_bgra, offset 0 is B, 1 is G, 2 is R.
  // If we want RGB output, NCHW channel 0 should pull from R (offset 2).
  int ch_map[3];
  if (cfg.assume_bgra) {
    if (cfg.to_rgb) {
      ch_map[0] = 2; // R
      ch_map[1] = 1; // G
      ch_map[2] = 0; // B
    } else {
      ch_map[0] = 0; // B
      ch_map[1] = 1; // G
      ch_map[2] = 2; // R
    }
  } else {
    // Assume RGBA buffer layout
    if (cfg.to_rgb) {
      ch_map[0] = 0; // R
      ch_map[1] = 1; // G
      ch_map[2] = 2; // B
    } else {
      ch_map[0] = 2; // B
      ch_map[1] = 1; // G
      ch_map[2] = 0; // R
    }
  }

  size_t channel_stride = static_cast<size_t>(cfg.out_w) * cfg.out_h;

  for (int y = 0; y < cfg.out_h; ++y) {
    for (int x = 0; x < cfg.out_w; ++x) {
      float src_x, src_y;
      GetSourceIndices(cfg.out_w, cfg.out_h, in_w, in_h, cfg.resize, x, y,
                       src_x, src_y);

      int src_x_nearest =
          std::max(0, std::min(in_w - 1, static_cast<int>(std::floor(src_x))));
      int src_y_nearest =
          std::max(0, std::min(in_h - 1, static_cast<int>(std::floor(src_y))));
      size_t nearest_idx = (src_y_nearest * in_w + src_x_nearest) * 4;

      for (int c = 0; c < 3; ++c) {
        float val;
        int src_offset = ch_map[c];

        if (cfg.resize == PreprocessConfig::BILINEAR) {
          BilinearInterpolate(in_bgra, in_w, in_h, src_x, src_y, src_offset,
                              val);
        } else {
          val = static_cast<float>(in_bgra[nearest_idx + src_offset]);
        }

        // Normalize and write to NCHW flat layout
        float norm_val = (val / 255.0f - cfg.mean[c]) / cfg.std[c];
        out_nchw[c * channel_stride + y * cfg.out_w + x] = norm_val;
      }
    }
  }

  return true;
}
