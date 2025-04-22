#include "display.hpp"

#include <SDL3/SDL_vulkan.h>

void Display::init() {
  SDL_ASSERT(SDL_Init(SDL_INIT_VIDEO));
  SDL_ASSERT(SDL_Vulkan_LoadLibrary(nullptr));

  int32_t displayCount;
  SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);

  SDL_ASSERT(displays);

  const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(displays[0]);
  
  SDL_ASSERT(displayMode);

  width = displayMode->w;
  height = displayMode->h;

  window = SDL_CreateWindow("Vulkan", width, height,
                            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  
  SDL_ASSERT(window);

  uint32_t extensionCount{};
  SDL_ASSERT(SDL_Vulkan_GetInstanceExtensions(&extensionCount));

  auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  vulkanExtensions.resize(extensionCount);

  for (size_t i = 0; i < extensionCount; i++) {
    vulkanExtensions[i] = extensions[i];
  }

  SDL_ASSERT(vulkanExtensions.size() == extensionCount);
}

void Display::destroy() {
  SDL_DestroyWindow(window);
  SDL_Vulkan_UnloadLibrary();
  SDL_Quit();
}

VkSurfaceKHR Display::createVulkanSurface(const VkInstance& instance) {
  VkSurfaceKHR surface{};
  SDL_ASSERT(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));
  return surface;
}
