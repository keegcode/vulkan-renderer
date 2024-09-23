#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdexcept>

#include "sdl-display.hpp"

void Display::init() {
  if (SDL_Init(SDL_INIT_EVERYTHING)) {
    throw std::runtime_error{std::string{"Failed to init SDL: "} + SDL_GetError()};
  }

  if (SDL_Vulkan_LoadLibrary(nullptr)) {
    throw std::runtime_error{std::string{"Failed to init Vulkan for SDL: "} + SDL_GetError()};
  }
  
  if (SDL_GetDisplayMode(0, 0, &displayMode)) {
    throw std::runtime_error{std::string{"Failed to get display mode: "} + SDL_GetError()};
  }

  window = SDL_CreateWindow("Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, displayMode.w, displayMode.h, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    throw std::runtime_error{std::string{"Failed to create SDL window: "} + SDL_GetError()};
  }

  uint32_t extensionCount{};
  if (SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr) == SDL_FALSE) {
    throw std::runtime_error{std::string{"Failed to load SDL Vulkan extensions: "} + SDL_GetError()};
  }

  vulkanExtensions.resize(extensionCount);
  if (SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, vulkanExtensions.data()) == SDL_FALSE) {
    throw std::runtime_error{std::string{"Failed to load SDL Vulkan extensions: "} + SDL_GetError()};
  }
};

void Display::destroy() {
  SDL_DestroyWindow(window);
  SDL_Vulkan_UnloadLibrary();
  SDL_Quit();
};

VkSurfaceKHR Display::createVulkanSurface(const VkInstance& instance) {
  VkSurfaceKHR surface{};
  if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
    throw std::runtime_error{std::string{"Failed to create Vulkan/SDL surface: "} + SDL_GetError()};
  }
  return surface;
};

