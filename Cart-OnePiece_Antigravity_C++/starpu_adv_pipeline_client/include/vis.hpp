#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vis {

// Convert standard CARLA BGRA (4 channels) to RGB (3 channels)
void BgraToRgb(const uint8_t *bgra, int w, int h, uint8_t *out_rgb);

// Colorize 1D semantic labels (e.g. from output of network or CARLA GT) into
// RGB using the standard CityScapes palette.
void ColorizeCityscapes(const uint8_t *labels, int w, int h, uint8_t *out_rgb);

// Blend an RGB image with a segmentation RGB map using an alpha factor.
// formula: out = (seg * alpha) + (rgb * (1 - alpha))
void BlendOverlay(const uint8_t *rgb, const uint8_t *seg_rgb, int w, int h,
                  float alpha, uint8_t *out_rgb);

// Upsample a 1D label array from (in_w, in_h) to (out_w, out_h) using Nearest
// Neighbor.
void UpsampleNearest(const uint8_t *in_labels, int in_w, int in_h,
                     uint8_t *out_labels, int out_w, int out_h);

// Arrange four separate RGB buffers (w x h) into a single 2x2 grid (2*w x 2*h).
// Layout: top-left (rgb_tl), top-right (rgb_tr), bottom-left (rgb_bl),
// bottom-right (rgb_br).
void CreateMosaic(const uint8_t *rgb_tl, const uint8_t *rgb_tr,
                  const uint8_t *rgb_bl, const uint8_t *rgb_br, int w, int h,
                  uint8_t *out_mosaic);

// Save Image as PNG wrapper
bool SavePng(const std::string &filepath, int w, int h, int channels,
             const uint8_t *data);

} // namespace vis
