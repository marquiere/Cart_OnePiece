#include "tensor_stats.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

std::vector<TensorStats> ComputeTensorStatsNCHW(const float *nchw_data,
                                                int channels, int width,
                                                int height) {
  if (!nchw_data || channels <= 0 || width <= 0 || height <= 0) {
    return {};
  }

  std::vector<TensorStats> result(channels);
  size_t channel_stride = static_cast<size_t>(width) * height;

  for (int c = 0; c < channels; ++c) {
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    double sum = 0.0;

    size_t offset = c * channel_stride;
    for (size_t i = 0; i < channel_stride; ++i) {
      float val = nchw_data[offset + i];

      if (val < min_val)
        min_val = val;
      if (val > max_val)
        max_val = val;

      if (!std::isnan(val) && !std::isinf(val)) {
        sum += val;
      }
    }

    result[c].min = min_val;
    result[c].max = max_val;
    result[c].mean = static_cast<float>(sum / channel_stride);
  }

  return result;
}

void PrintTensorStats(const std::vector<TensorStats> &stats) {
  std::cout << "--- NCHW Tensor Stats (Per Channel) ---" << std::endl;
  for (size_t c = 0; c < stats.size(); ++c) {
    std::cout << "Channel " << c << " |\tMin: " << std::fixed
              << std::setprecision(4) << stats[c].min
              << "  \tMax: " << stats[c].max << "  \tMean: " << stats[c].mean
              << std::endl;
  }
}
