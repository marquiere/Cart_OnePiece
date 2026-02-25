#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

// Forward declarations to avoid bringing SDL.h into the header
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class SegmentationViewerSDL2 {
public:
  SegmentationViewerSDL2();
  ~SegmentationViewerSDL2();

  // Initialize the SDL2 window. Returns true on success.
  bool init(int width, int height, float window_scale = 1.0f);

  // Non-blocking frame submission. Safely copies the RGB data.
  void submit_frame_rgb888(const uint8_t *data, int w, int h);

  // Process SDL events (e.g. window close)
  // Returns true if the user requested to quit.
  bool poll_events();

  // Renders the latest submitted frame to the screen
  void render_latest();

  // Shutdown and clean up resources
  void shutdown();

private:
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;

  int width_ = 0;
  int height_ = 0;
  bool initialized_ = false;

  // Mutex to protect the frame buffer across thread boundaries
  std::mutex frame_mutex_;
  std::vector<uint8_t> frame_buffer_;
  bool has_new_frame_ = false;
};
