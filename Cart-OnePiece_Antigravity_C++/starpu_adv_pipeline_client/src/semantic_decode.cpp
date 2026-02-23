#include "semantic_decode.hpp"
#include <algorithm>

std::vector<uint8_t> DecodeSemanticLabels(const uint8_t *raw, int w, int h,
                                          bool assume_bgra) {
  size_t num_pixels = static_cast<size_t>(w * h);
  std::vector<uint8_t> labels(num_pixels);

  // CARLA Semantic Segmentation: label is in the Red channel.
  size_t channel_offset = assume_bgra ? 2 : 0;

  for (size_t i = 0; i < num_pixels; ++i) {
    labels[i] = raw[(i * 4) + channel_offset];
  }

  return labels;
}

SemanticStats ComputeSemanticStats(const std::vector<uint8_t> &labels) {
  SemanticStats stats;
  stats.histogram.assign(256, 0);

  for (uint8_t label : labels) {
    stats.histogram[label]++;
  }

  for (int i = 0; i < 256; ++i) {
    if (stats.histogram[i] > 0) {
      stats.unique_labels.push_back(static_cast<uint8_t>(i));
      stats.top_k.push_back({static_cast<uint8_t>(i), stats.histogram[i]});
    }
  }

  // Sort unique labels ascending
  std::sort(stats.unique_labels.begin(), stats.unique_labels.end());

  // Sort top_k by count descending
  std::sort(
      stats.top_k.begin(), stats.top_k.end(),
      [](const std::pair<uint8_t, size_t> &a,
         const std::pair<uint8_t, size_t> &b) { return a.second > b.second; });

  // Keep only top 10
  if (stats.top_k.size() > 10) {
    stats.top_k.resize(10);
  }

  return stats;
}
