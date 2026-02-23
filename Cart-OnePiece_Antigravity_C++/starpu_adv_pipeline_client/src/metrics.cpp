#include "metrics.hpp"
#include <iostream>
#include <unordered_map>

namespace metrics {

double PixelAccuracy(const uint8_t *pred, const uint8_t *gt, int w, int h) {
  if (!pred || !gt || w <= 0 || h <= 0)
    return 0.0;

  size_t total = static_cast<size_t>(w) * h;
  size_t correct = 0;

  for (size_t i = 0; i < total; ++i) {
    if (pred[i] == gt[i])
      correct++;
  }

  return static_cast<double>(correct) / static_cast<double>(total);
}

double MeanIoU(const uint8_t *pred, const uint8_t *gt, int w, int h,
               const std::vector<uint8_t> &classes_to_eval,
               EvalResult *out_detailed) {
  if (!pred || !gt || w <= 0 || h <= 0 || classes_to_eval.empty()) {
    if (out_detailed) {
      out_detailed->pixel_accuracy = 0.0;
      out_detailed->miou = 0.0;
      out_detailed->per_class_iou.assign(256, -1.0);
    }
    return 0.0;
  }

  size_t total = static_cast<size_t>(w) * h;

  // Arrays instead of hash maps for speed (0-255 categories)
  std::vector<size_t> intersection(256, 0);
  std::vector<size_t> union_count(256, 0);

  for (size_t i = 0; i < total; ++i) {
    uint8_t p = pred[i];
    uint8_t g = gt[i];

    union_count[p]++;
    union_count[g]++;
    if (p == g) {
      intersection[g]++;
      union_count[g]--; // Avoid double counting
    }
  }

  double total_iou = 0.0;
  int valid_classes = 0;

  if (out_detailed) {
    out_detailed->per_class_iou.assign(256, -1.0);
  }

  for (uint8_t c : classes_to_eval) {
    double iou = 0.0;
    if (union_count[c] > 0) {
      iou = static_cast<double>(intersection[c]) /
            static_cast<double>(union_count[c]);
      total_iou += iou;
      valid_classes++;
    }
    if (out_detailed && union_count[c] > 0) {
      out_detailed->per_class_iou[c] = iou;
    }
  }

  double miou = valid_classes > 0 ? total_iou / valid_classes : 0.0;

  if (out_detailed) {
    out_detailed->miou = miou;
    out_detailed->pixel_accuracy = PixelAccuracy(pred, gt, w, h);
  }

  return miou;
}

EvalResult EvaluateFrame(const uint8_t *pred, const uint8_t *gt, int w, int h,
                         bool exclude_zero) {
  EvalResult result;
  result.pixel_accuracy = 0.0;
  result.miou = 0.0;
  result.per_class_iou.assign(256, -1.0);

  if (!pred || !gt || w <= 0 || h <= 0)
    return result;

  size_t total = static_cast<size_t>(w) * h;
  std::vector<uint8_t> present_in_gt(256, 0);

  for (size_t i = 0; i < total; ++i) {
    present_in_gt[gt[i]] = 1;
  }

  std::vector<uint8_t> classes_to_eval;
  classes_to_eval.reserve(256);
  for (int i = 0; i < 256; ++i) {
    if (exclude_zero && i == 0)
      continue;
    if (present_in_gt[i]) {
      classes_to_eval.push_back(static_cast<uint8_t>(i));
    }
  }

  MeanIoU(pred, gt, w, h, classes_to_eval, &result);
  return result;
}

} // namespace metrics
