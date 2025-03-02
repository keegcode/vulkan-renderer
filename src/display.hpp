#pragma once

#include <SDL.h>
#include <vulkan/vulkan_core.h>
#include <vector>

class Display {
 public:
  SDL_Window* window;
  SDL_DisplayMode displayMode;
  std::vector<const char*> vulkanExtensions;

  void init();
  void destroy();
  VkSurfaceKHR createVulkanSurface(const VkInstance& instance);
};
