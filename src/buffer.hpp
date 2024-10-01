#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

class Buffer {
public:
  vk::Buffer buffer;
  VmaAllocation allocation;
  uint32_t size;

  Buffer();
  Buffer(const VmaAllocator& allocator, const uint32_t size, const vk::BufferUsageFlagBits usage);
  Buffer(const VmaAllocator& allocator, const void* data, const uint32_t size, const vk::BufferUsageFlagBits usage);

  void copyToImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Image& image, unsigned char* srcData, const vk::Extent3D& extent);

  void destroy(const VmaAllocator& allocator);
};
