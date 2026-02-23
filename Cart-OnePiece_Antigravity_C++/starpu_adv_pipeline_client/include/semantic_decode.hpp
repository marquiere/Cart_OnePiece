#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

struct SemanticStats {
  std::vector<uint8_t> unique_labels;
  std::vector<size_t> histogram; // index is label_id, value is pixel count
  std::vector<std::pair<uint8_t, size_t>> top_k; // sorted by count descending
};

// Decodes a raw 4-channel pixel buffer into a 1-channel label buffer.
// CARLA Semantic Segmentation uses the RED channel for the label ID.
// assume_bgra=true means Red is at byte index 2: [B, G, R, A]
// assume_bgra=false means Red is at byte index 0: [R, G, B, A]
std::vector<uint8_t> DecodeSemanticLabels(const uint8_t *raw, int w, int h,
                                          bool assume_bgra);

// Computes unique labels, full histogram, and top-k labels from a 1D label
// buffer.
SemanticStats ComputeSemanticStats(const std::vector<uint8_t> &labels);
