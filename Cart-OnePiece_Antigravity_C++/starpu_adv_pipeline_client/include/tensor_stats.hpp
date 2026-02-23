#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct TensorStats {
  float min;
  float max;
  float mean;
};

// Computes min/max/mean per channel for a given NCHW-ordered flat tensor array
std::vector<TensorStats> ComputeTensorStatsNCHW(const float *nchw_data,
                                                int channels, int width,
                                                int height);

// Helper to nicely print the stats
void PrintTensorStats(const std::vector<TensorStats> &stats);
