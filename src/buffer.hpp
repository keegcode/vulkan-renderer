#pragma once

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>

class Buffer {
public:
  vk::Buffer buffer;
  VmaAllocation allocation;
  vk::DeviceSize size;

  Buffer();

  Buffer(const VmaAllocator &allocator, const vk::DeviceSize size,
         const vk::BufferUsageFlagBits usage);

  Buffer(const VmaAllocator &allocator, const void *data,
         const vk::DeviceSize size, const vk::BufferUsageFlagBits usage);

  void copyToImage(const VmaAllocator &allocator, const vk::Device &device,
                   const vk::CommandPool &commandPool,
                   const vk::Queue &transferQueue, const vk::Image &image,
                   unsigned char *srcData, const vk::Extent3D &extent);

  void destroy(const VmaAllocator &allocator);

  vk::DeviceAddress getDeviceAddress(const vk::Device &device);
};
