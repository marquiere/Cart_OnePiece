#include "frame_sync.hpp"
#include <algorithm>
#include <iostream>

FrameSync::FrameSync(size_t window_size, size_t max_bytes,
                     uint64_t max_age_frames)
    : window_size_(window_size), max_bytes_(max_bytes),
      max_age_frames_(max_age_frames) {}

void FrameSync::PushRgb(FrameIn &&rgb) {
  std::lock_guard<std::mutex> lock(mu_);
  stats_.pushed_rgb++;

  uint64_t fid = rgb.frame_id;
  rgb_map_[fid] = std::move(rgb);
  rgb_history_.push(fid);

  CheckMatchLocked(fid);
  HousekeepingLocked();
}

void FrameSync::PushGt(GtFrame &&gt) {
  std::lock_guard<std::mutex> lock(mu_);
  stats_.pushed_gt++;

  uint64_t fid = gt.frame_id;
  gt_map_[fid] = std::move(gt);
  gt_history_.push(fid);

  CheckMatchLocked(fid);
  HousekeepingLocked();
}

bool FrameSync::TryPopMatched(MatchedPair &out) {
  std::lock_guard<std::mutex> lock(mu_);
  if (matched_queue_.empty())
    return false;

  out = std::move(matched_queue_.front());
  matched_queue_.pop();
  return true;
}

SyncStats FrameSync::GetStats() {
  std::lock_guard<std::mutex> lock(mu_);
  stats_.currently_buffered_rgb = rgb_map_.size();
  stats_.currently_buffered_gt = gt_map_.size();
  return stats_;
}

void FrameSync::CheckMatchLocked(uint64_t frame_id) {
  auto rgb_it = rgb_map_.find(frame_id);
  auto gt_it = gt_map_.find(frame_id);

  if (rgb_it != rgb_map_.end() && gt_it != gt_map_.end()) {
    if (matched_queue_.size() >= matched_queue_limit_) {
      // Optional: dropping oldest match to enforce ceiling on matched queue
      matched_queue_.pop();
    }

    MatchedPair pair;
    pair.frame_id = frame_id;
    pair.rgb = std::move(rgb_it->second);
    pair.gt = std::move(gt_it->second);

    matched_queue_.push(std::move(pair));
    stats_.matched++;
    stats_.last_matched_frame_id = frame_id;

    rgb_map_.erase(rgb_it);
    gt_map_.erase(gt_it);
  }
}

void FrameSync::HousekeepingLocked() {
  // Drop RGB if map > window
  while (rgb_map_.size() > window_size_ && !rgb_history_.empty()) {
    uint64_t oldest_fid = rgb_history_.front();
    rgb_history_.pop();
    if (rgb_map_.erase(oldest_fid) > 0) {
      stats_.dropped_rgb_overflow++;
    }
  }

  // Drop GT if map > window
  while (gt_map_.size() > window_size_ && !gt_history_.empty()) {
    uint64_t oldest_fid = gt_history_.front();
    gt_history_.pop();
    if (gt_map_.erase(oldest_fid) > 0) {
      stats_.dropped_gt_overflow++;
    }
  }

  // Optional Age dropping
  if (max_age_frames_ > 0 && stats_.last_matched_frame_id > 0) {
    // Clear old RGBs
    while (!rgb_history_.empty()) {
      uint64_t fid = rgb_history_.front();
      if (stats_.last_matched_frame_id > fid &&
          (stats_.last_matched_frame_id - fid) > max_age_frames_) {
        rgb_history_.pop();
        if (rgb_map_.erase(fid) > 0)
          stats_.dropped_rgb_age++;
      } else {
        break;
      }
    }
    // Clear old GTs
    while (!gt_history_.empty()) {
      uint64_t fid = gt_history_.front();
      if (stats_.last_matched_frame_id > fid &&
          (stats_.last_matched_frame_id - fid) > max_age_frames_) {
        gt_history_.pop();
        if (gt_map_.erase(fid) > 0)
          stats_.dropped_gt_age++;
      } else {
        break;
      }
    }
  }
}
