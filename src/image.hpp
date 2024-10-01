#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

class Image {
public:
  vk::Image image;
  vk::ImageView view;
  VmaAllocation allocation;
  vk::Extent3D extent;
  vk::ImageLayout layout = vk::ImageLayout::eUndefined;

  Image();
  Image(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Extent3D& extent, const vk::Format format, const vk::ImageUsageFlagBits usage, const vk::ImageAspectFlagBits aspectMask);
  Image(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const std::string_view path, const vk::ImageLayout layout);

  void transitionImageLayout(const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::ImageLayout& newLayout);

  void destroy(const VmaAllocator& allocator, const vk::Device& device);
};
