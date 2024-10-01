#pragma once

#include <vulkan/vulkan.hpp>

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

namespace utils {
  vkb::Swapchain createSwapchain(vkb::Device device, vk::Extent2D extent, uint16_t minImageCount, vkb::Swapchain* old);
  std::tuple<vk::Viewport, vk::Rect2D> createViewportAndScissors(const vk::Extent3D& extent);
  vk::CommandBuffer beginSingleSubmitCommand(const vk::Device& device, const vk::CommandPool& commandPool);
  void endSingleSubmitCommand(const vk::Device& device, const vk::CommandPool& commandPool, const vk::CommandBuffer& commandBuffer, const vk::Queue& queue);
}
