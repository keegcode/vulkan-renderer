#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan_core.h>
#include <vector>
#include <cstdio>
#include "debugbreak.h"

#define SDL_ASSERT(expr) \
  if (expr) { \
  } else { \
    printf("Assertion `%s` failed.\nError: %s\n", #expr, SDL_GetError()); DEBUG_BREAK(); \
  }

class Display {
 public:
  SDL_Window* window;
  std::vector<const char*> vulkanExtensions;

  uint32_t width;
  uint32_t height;

  void init();
  void destroy();
  VkSurfaceKHR createVulkanSurface(const VkInstance& instance);
};
