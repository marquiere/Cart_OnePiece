#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace metrics {

struct EvalResult {
  double pixel_accuracy;
  double miou;
  std::vector<double> per_class_iou; // indexed by class ID (0 to 255). -1.0
                                     // means not present in GT.
};

// Compute Pixel Accuracy between prediction and ground truth
double PixelAccuracy(const uint8_t *pred, const uint8_t *gt, int w, int h);

// Compute mIoU over a specific list of classes
double MeanIoU(const uint8_t *pred, const uint8_t *gt, int w, int h,
               const std::vector<uint8_t> &classes_to_eval,
               EvalResult *out_detailed = nullptr);

// Computes mIoU automatically masking out classes not present in the GT image
// (or overall optionally filtering class 0 if requested)
EvalResult EvaluateFrame(const uint8_t *pred, const uint8_t *gt, int w, int h,
                         bool exclude_zero = false);

} // namespace metrics
