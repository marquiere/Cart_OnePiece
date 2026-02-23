#include "postprocess.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace postprocess {

void ArgmaxLogitsToLabels(const float *logits, int c, int h, int w,
                          std::vector<uint8_t> &out_labels) {
  if (!logits || c <= 0 || h <= 0 || w <= 0)
    return;

  size_t spatial_size = static_cast<size_t>(h) * w;
  if (out_labels.size() != spatial_size) {
    out_labels.resize(spatial_size);
  }

// Usually output logits are [1, C, H, W]. So shape is C*H*W in memory.
// For a specific pixel at (y,x), the array of class scores is spaced by H*W.
#pragma omp parallel for
  for (size_t i = 0; i < spatial_size; ++i) {
    int best_c = 0;
    float best_val = -std::numeric_limits<float>::infinity();

    for (int ch = 0; ch < c; ++ch) {
      float val = logits[ch * spatial_size + i];
      if (val > best_val) {
        best_val = val;
        best_c = ch;
      }
    }
    // Clip best_c to 255 to fit in uint8_t
    out_labels[i] = static_cast<uint8_t>(std::min(best_c, 255));
  }
}

void DirectLabelsToUint8(const int32_t *in_labels, int h, int w,
                         std::vector<uint8_t> &out_labels) {
  if (!in_labels || h <= 0 || w <= 0)
    return;

  size_t spatial_size = static_cast<size_t>(h) * w;
  if (out_labels.size() != spatial_size) {
    out_labels.resize(spatial_size);
  }

#pragma omp parallel for
  for (size_t i = 0; i < spatial_size; ++i) {
    out_labels[i] =
        static_cast<uint8_t>(std::min(std::max(in_labels[i], 0), 255));
  }
}

} // namespace postprocess
