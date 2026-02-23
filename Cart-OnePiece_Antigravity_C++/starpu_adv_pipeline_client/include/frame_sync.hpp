#pragma once

#include <boost/shared_ptr.hpp>
#include <carla/sensor/data/Image.h>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

// A simple structure representing an incoming RGB frame from the sensor.
struct FrameIn {
  uint64_t frame_id;
  double timestamp;
  int w;
  int h;
  boost::shared_ptr<carla::sensor::data::Image> carla_image;
};

// A simple structure representing an incoming GT Semantic frame from the
// sensor.
struct GtFrame {
  uint64_t frame_id;
  double timestamp;
  int w;
  int h;
  boost::shared_ptr<carla::sensor::data::Image> carla_image;
};

// Represents a successfully matched pair.
struct MatchedPair {
  uint64_t frame_id;
  FrameIn rgb;
  GtFrame gt;
};

// Statistics returned by the synchronizer to monitor health and debugging.
struct SyncStats {
  size_t pushed_rgb = 0;
  size_t pushed_gt = 0;
  size_t matched = 0;

  size_t dropped_rgb_overflow = 0;
  size_t dropped_gt_overflow = 0;

  size_t dropped_rgb_age = 0;
  size_t dropped_gt_age = 0;

  size_t currently_buffered_rgb = 0;
  size_t currently_buffered_gt = 0;

  uint64_t last_matched_frame_id = 0;
};

class FrameSync {
public:
  // constructor sets the constraints
  FrameSync(size_t window_size, size_t max_bytes = 0,
            uint64_t max_age_frames = 0);
  ~FrameSync() = default;

  // Push APIs for the sensor callbacks
  void PushRgb(FrameIn &&rgb);
  void PushGt(GtFrame &&gt);

  // Consumer API to retrieve matched pairs (non-blocking, returns false if none
  // ready)
  bool TryPopMatched(MatchedPair &out);

  // Diagnostic API
  SyncStats GetStats();

private:
  void CheckMatchLocked(uint64_t frame_id);
  void HousekeepingLocked();

  size_t window_size_;
  size_t max_bytes_; // Optional memory bound on the maps
  uint64_t max_age_frames_;

  std::mutex mu_;
  std::unordered_map<uint64_t, FrameIn> rgb_map_;
  std::unordered_map<uint64_t, GtFrame> gt_map_;

  // We keep an ordered list of what we hold to easily prune "oldest" when
  // window is exceeded
  std::queue<uint64_t> rgb_history_;
  std::queue<uint64_t> gt_history_;

  std::queue<MatchedPair> matched_queue_;
  size_t matched_queue_limit_ = 200; // Hard limit on the output queue length

  SyncStats stats_;
};
