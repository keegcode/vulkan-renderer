#include "display.hpp"

#include <SDL3/SDL_vulkan.h>
#include <stdexcept>

void Display::init() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error{std::string{"Failed to init SDL: "} +
                             SDL_GetError()};
  }

  if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    throw std::runtime_error{std::string{"Failed to init Vulkan for SDL: "} +
                             SDL_GetError()};
  }

  int32_t displayCount;
  SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);

  if (!displays) {
    throw std::runtime_error{
        std::string{"Failed to get a list of displays for SDL: "} +
        SDL_GetError()};
  }

  const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(displays[0]);

  if (displayMode == nullptr) {
    throw std::runtime_error{std::string{"Failed to get Display Mode: "} +
                             SDL_GetError()};
  }

  width = displayMode->w;
  height = displayMode->h;

  window = SDL_CreateWindow("Vulkan", width, height,
                            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  if (!window) {
    throw std::runtime_error{std::string{"Failed to create SDL window: "} +
                             SDL_GetError()};
  }

  uint32_t extensionCount{};
  if (!SDL_Vulkan_GetInstanceExtensions(&extensionCount)) {
    throw std::runtime_error{
        std::string{"Failed to load SDL Vulkan extensions: "} + SDL_GetError()};
  }

  auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  vulkanExtensions.resize(extensionCount);

  for (size_t i = 0; i < extensionCount; i++) {
    vulkanExtensions[i] = extensions[i];
  }

  if (vulkanExtensions.size() < extensionCount) {
    throw std::runtime_error{
        std::string{"Failed to load SDL Vulkan extensions: "} + SDL_GetError()};
  }
}

void Display::destroy() {
  SDL_DestroyWindow(window);
  SDL_Vulkan_UnloadLibrary();
  SDL_Quit();
}

VkSurfaceKHR Display::createVulkanSurface(const VkInstance& instance) {
  VkSurfaceKHR surface{};
  if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
    throw std::runtime_error{
        std::string{"Failed to create Vulkan/SDL surface: "} + SDL_GetError()};
  }
  return surface;
}
