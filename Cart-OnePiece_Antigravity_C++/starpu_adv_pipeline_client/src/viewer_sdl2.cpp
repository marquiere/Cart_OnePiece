#include "viewer_sdl2.hpp"
#include <SDL2/SDL.h>
#include <cstring>
#include <iostream>

SegmentationViewerSDL2::SegmentationViewerSDL2() {}

SegmentationViewerSDL2::~SegmentationViewerSDL2() { shutdown(); }

bool SegmentationViewerSDL2::init(int width, int height, float window_scale) {
  if (initialized_)
    return true;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "[Viewer] SDL could not initialize! SDL_Error: "
              << SDL_GetError() << "\n";
    return false;
  }

  int win_w = static_cast<int>(width * window_scale);
  int win_h = static_cast<int>(height * window_scale);

  window_ =
      SDL_CreateWindow("Real-Time Segmentation Viewer", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, win_w, win_h, SDL_WINDOW_SHOWN);
  if (!window_) {
    std::cerr << "[Viewer] Window could not be created! SDL_Error: "
              << SDL_GetError() << "\n";
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer_) {
    std::cerr << "[Viewer] Renderer could not be created! SDL_Error: "
              << SDL_GetError() << "\n";
    return false;
  }

  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24,
                               SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture_) {
    std::cerr << "[Viewer] Texture could not be created! SDL_Error: "
              << SDL_GetError() << "\n";
    return false;
  }

  width_ = width;
  height_ = height;
  frame_buffer_.resize(width * height * 3, 0); // Black initial frame
  initialized_ = true;

  return true;
}

void SegmentationViewerSDL2::submit_frame_rgb888(const uint8_t *data, int w,
                                                 int h) {
  if (!initialized_)
    return;
  if (w != width_ || h != height_)
    return;

  std::lock_guard<std::mutex> lock(frame_mutex_);
  std::memcpy(frame_buffer_.data(), data, w * h * 3);
  has_new_frame_ = true;
}

bool SegmentationViewerSDL2::poll_events() {
  if (!initialized_)
    return false;

  SDL_Event e;
  while (SDL_PollEvent(&e) != 0) {
    if (e.type == SDL_QUIT) {
      return true;
    }
  }
  return false;
}

void SegmentationViewerSDL2::render_latest() {
  if (!initialized_)
    return;

  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (has_new_frame_) {
      SDL_UpdateTexture(texture_, nullptr, frame_buffer_.data(), width_ * 3);
      has_new_frame_ = false;
    }
  }

  SDL_RenderClear(renderer_);
  SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
  SDL_RenderPresent(renderer_);
}

void SegmentationViewerSDL2::shutdown() {
  if (!initialized_)
    return;

  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }

  SDL_Quit();
  initialized_ = false;
}
