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

  // Cityscapes TrainID (0-18) to CARLA Raw ID (approximate lookup table)
  // Maps classes like Road(0)->7, Building(2)->1, Vehicle(13)->10, etc.
  static const uint8_t TRAINID_TO_CARLA_RAW[] = {
      7,  // 0: Road
      8,  // 1: Sidewalk
      1,  // 2: Building
      11, // 3: Wall
      2,  // 4: Fence
      5,  // 5: Pole
      18, // 6: TrafficLight
      12, // 7: TrafficSign
      9,  // 8: Vegetation
      22, // 9: Terrain
      13, // 10: Sky
      4,  // 11: Person (Pedestrian)
      4,  // 12: Rider
      10, // 13: Car (Vehicle)
      10, // 14: Truck
      10, // 15: Bus
      10, // 16: Train
      10, // 17: Motorcycle
      10  // 18: Bicycle
  };
  
  // Pascal VOC (21) to CARLA Raw ID approximation
  // VOC doesn't have "road" or "building", so they fall into background (0).
  static const uint8_t VOC21_TO_CARLA_RAW[] = {
      0,  // 0: background
      0,  // 1: aeroplane
      10, // 2: bicycle
      0,  // 3: bird
      0,  // 4: boat
      0,  // 5: bottle
      10, // 6: bus
      10, // 7: car
      0,  // 8: cat
      0,  // 9: chair
      0,  // 10: cow
      0,  // 11: diningtable
      0,  // 12: dog
      0,  // 13: horse
      10, // 14: motorbike
      4,  // 15: person
      9,  // 16: pottedplant
      0,  // 17: sheep
      0,  // 18: sofa
      10, // 19: train
      0   // 20: tvmonitor
  };

  bool is_cityscapes_19 = (c == 19);
  bool is_voc_21 = (c == 21);

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

    // Bounds guard: predicted label must be within [0, c-1]
    best_c = std::max(0, std::min(best_c, c - 1));

    uint8_t final_label = static_cast<uint8_t>(best_c);
    if (is_cityscapes_19 && best_c < 19) {
      final_label = TRAINID_TO_CARLA_RAW[best_c];
    } else if (is_voc_21 && best_c < 21) {
      final_label = VOC21_TO_CARLA_RAW[best_c];
    }

    out_labels[i] = final_label;
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
