#include "vis.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstring>
#include <iostream>

namespace vis {

// Cityscapes color palette (approx 23 classes, similar to CARLA)
// Format is R, G, B
static const uint8_t CITYSCAPES_PALETTE[][3] = {
    {0, 0, 0},       // 0: Unlabeled
    {70, 70, 70},    // 1: Building
    {100, 40, 40},   // 2: Fence
    {55, 90, 80},    // 3: Other
    {220, 20, 60},   // 4: Pedestrian
    {153, 153, 153}, // 5: Pole
    {157, 234, 50},  // 6: RoadLine
    {128, 64, 128},  // 7: Road
    {244, 35, 232},  // 8: Sidewalk
    {107, 142, 35},  // 9: Vegetation
    {0, 0, 142},     // 10: Vehicle
    {102, 102, 156}, // 11: Wall
    {220, 220, 0},   // 12: TrafficSign
    {70, 130, 180},  // 13: Sky
    {81, 0, 81},     // 14: Ground
    {150, 100, 100}, // 15: Bridge
    {230, 150, 140}, // 16: RailTrack
    {180, 165, 180}, // 17: GuardRail
    {250, 170, 30},  // 18: TrafficLight
    {110, 190, 160}, // 19: Static
    {170, 120, 50},  // 20: Dynamic
    {45, 60, 150},   // 21: Water
    {145, 170, 100}  // 22: Terrain
};
static const int PALETTE_SIZE =
    sizeof(CITYSCAPES_PALETTE) / sizeof(CITYSCAPES_PALETTE[0]);

void BgraToRgb(const uint8_t *bgra, int w, int h, uint8_t *out_rgb) {
  int pixels = w * h;
  for (int i = 0; i < pixels; ++i) {
    out_rgb[i * 3 + 0] = bgra[i * 4 + 2]; // R
    out_rgb[i * 3 + 1] = bgra[i * 4 + 1]; // G
    out_rgb[i * 3 + 2] = bgra[i * 4 + 0]; // B
  }
}

void ColorizeCityscapes(const uint8_t *labels, int w, int h, uint8_t *out_rgb) {
  int pixels = w * h;
  for (int i = 0; i < pixels; ++i) {
    uint8_t c = labels[i];
    if (c >= PALETTE_SIZE) {
      c = 0; // fallback to unlabeled if out of bounds
    }
    out_rgb[i * 3 + 0] = CITYSCAPES_PALETTE[c][0];
    out_rgb[i * 3 + 1] = CITYSCAPES_PALETTE[c][1];
    out_rgb[i * 3 + 2] = CITYSCAPES_PALETTE[c][2];
  }
}

void BlendOverlay(const uint8_t *rgb, const uint8_t *seg_rgb, int w, int h,
                  float alpha, uint8_t *out_rgb) {
  int pixels = w * h * 3;
  float beta = 1.0f - alpha;
  for (int i = 0; i < pixels; ++i) {
    float val = (seg_rgb[i] * alpha) + (rgb[i] * beta);
    out_rgb[i] = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, val)));
  }
}

void UpsampleNearest(const uint8_t *in_labels, int in_w, int in_h,
                     uint8_t *out_labels, int out_w, int out_h) {
  float x_ratio = static_cast<float>(in_w) / out_w;
  float y_ratio = static_cast<float>(in_h) / out_h;

  for (int y = 0; y < out_h; ++y) {
    for (int x = 0; x < out_w; ++x) {
      int px = static_cast<int>(x * x_ratio);
      int py = static_cast<int>(y * y_ratio);

      // bounds check to be safe
      px = std::max(0, std::min(px, in_w - 1));
      py = std::max(0, std::min(py, in_h - 1));

      out_labels[y * out_w + x] = in_labels[py * in_w + px];
    }
  }
}

void CreateMosaic(const uint8_t *rgb_tl, const uint8_t *rgb_tr,
                  const uint8_t *rgb_bl, const uint8_t *rgb_br, int w, int h,
                  uint8_t *out_mosaic) {
  int out_w = w * 2;
  int row_bytes = w * 3;
  int out_row_bytes = out_w * 3;

  for (int y = 0; y < h; ++y) {
    // Top half: TL and TR
    std::memcpy(out_mosaic + y * out_row_bytes, rgb_tl + y * row_bytes,
                row_bytes);
    std::memcpy(out_mosaic + y * out_row_bytes + row_bytes,
                rgb_tr + y * row_bytes, row_bytes);

    // Bottom half: BL and BR
    std::memcpy(out_mosaic + (y + h) * out_row_bytes, rgb_bl + y * row_bytes,
                row_bytes);
    std::memcpy(out_mosaic + (y + h) * out_row_bytes + row_bytes,
                rgb_br + y * row_bytes, row_bytes);
  }
}

void CreateMosaic8(const uint8_t *rgb_tl, const uint8_t *rgb_tml,
                   const uint8_t *rgb_tmr, const uint8_t *rgb_tr,
                   const uint8_t *rgb_bl, const uint8_t *rgb_bml,
                   const uint8_t *rgb_bmr, const uint8_t *rgb_br, int w, int h,
                   uint8_t *out_mosaic) {
  int out_w = w * 4;
  int row_bytes = w * 3;
  int out_row_bytes = out_w * 3;

  for (int y = 0; y < h; ++y) {
    // Top Row: TL, TML, TMR, TR
    std::memcpy(out_mosaic + y * out_row_bytes, rgb_tl + y * row_bytes,
                row_bytes);
    std::memcpy(out_mosaic + y * out_row_bytes + row_bytes,
                rgb_tml + y * row_bytes, row_bytes);
    std::memcpy(out_mosaic + y * out_row_bytes + row_bytes * 2,
                rgb_tmr + y * row_bytes, row_bytes);
    std::memcpy(out_mosaic + y * out_row_bytes + row_bytes * 3,
                rgb_tr + y * row_bytes, row_bytes);

    // Bottom Row: BL, BML, BMR, BR
    std::memcpy(out_mosaic + (y + h) * out_row_bytes, rgb_bl + y * row_bytes,
                row_bytes);
    std::memcpy(out_mosaic + (y + h) * out_row_bytes + row_bytes,
                rgb_bml + y * row_bytes, row_bytes);
    std::memcpy(out_mosaic + (y + h) * out_row_bytes + row_bytes * 2,
                rgb_bmr + y * row_bytes, row_bytes);
    std::memcpy(out_mosaic + (y + h) * out_row_bytes + row_bytes * 3,
                rgb_br + y * row_bytes, row_bytes);
  }
}

bool SavePng(const std::string &filepath, int w, int h, int channels,
             const uint8_t *data) {
  int res =
      stbi_write_png(filepath.c_str(), w, h, channels, data, w * channels);
  if (!res) {
    std::cerr << "Failed to write PNG: " << filepath << "\n";
    return false;
  }
  return true;
}

} // namespace vis
