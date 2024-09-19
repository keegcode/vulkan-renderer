#pragma once

#include <vulkan/vulkan.hpp>

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "vk-structs.hpp"

namespace utils {
  vkb::Swapchain createSwapchain(vkb::Device device, vk::Extent2D extent, uint16_t minImageCount);
  Image createImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const std::string_view path, vk::ImageLayout layout);
  std::tuple<vk::Viewport, vk::Rect2D> createViewportAndScissors(const vk::Extent3D& extent);
  Buffer createBuffer(VmaAllocator allocator, const void* data, const uint32_t size, vk::BufferUsageFlagBits usage);

  vk::CommandBuffer beginSingleSubmitCommand(const vk::Device& device, const vk::CommandPool& commandPool);
  void endSingleSubmitCommand(const vk::Device& device, const vk::CommandPool& commandPool, const vk::CommandBuffer& commandBuffer, const vk::Queue& queue);

  void transitionImageLayout(const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Image& image, const vk::ImageLayout& oldLayout, const vk::ImageLayout& newLayout);
  void copyBufferToImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, unsigned char* srcData, const vk::Extent3D& extent);
  void copyBufferToImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Image& image, unsigned char* srcData, const vk::Extent3D& extent);
}
