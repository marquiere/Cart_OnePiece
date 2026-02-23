#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace postprocess {

// Converts a batch of [1, C, H, W] Logits (FP32) into a [H, W] Label Map
// (uint8_t) using Argmax over the C dimension.
void ArgmaxLogitsToLabels(const float *logits, int c, int h, int w,
                          std::vector<uint8_t> &out_labels);

// Placeholder if engine outputs labels natively (already [1, H, W] or [H, W] as
// INT32)
void DirectLabelsToUint8(const int32_t *in_labels, int h, int w,
                         std::vector<uint8_t> &out_labels);

} // namespace postprocess
