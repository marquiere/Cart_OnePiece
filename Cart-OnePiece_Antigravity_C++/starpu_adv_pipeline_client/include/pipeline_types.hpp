#pragma once

#include <cstdint>
#include <vector>

struct TensorIn {
  std::vector<float> nchw_float32;
  int out_w = 0;
  int out_h = 0;
  uint64_t frame_id = 0;
};

struct PredOut {
  std::vector<uint8_t> label_map;
  int w = 0;
  int h = 0;
  uint64_t frame_id = 0;
};
