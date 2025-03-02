#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include "vk_mem_alloc.h"

class Buffer {
 public:
  vk::Buffer buffer;

  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;

  vk::DeviceSize size;

  Buffer();

  Buffer(const VmaAllocator& allocator,
         const vk::DeviceSize size,
         const vk::Flags<vk::BufferUsageFlagBits> usage);

  Buffer(const VmaAllocator& allocator,
         const void* data,
         const vk::DeviceSize size,
         const vk::Flags<vk::BufferUsageFlagBits> usage);

  Buffer(const VmaAllocator& allocator,
         const vk::DeviceSize s,
         const vk::Flags<vk::BufferUsageFlagBits> usage,
         VmaMemoryUsage memoryUsage,
         VmaAllocationCreateFlags createFlags);

  static Buffer createUniformBuffer(const VmaAllocator& allocator,
                                    const vk::DeviceSize size) {
    return Buffer{allocator, size,
                  vk::BufferUsageFlagBits::eUniformBuffer |
                      vk::BufferUsageFlagBits::eShaderDeviceAddress};
  }

  static void copyToImage(const VmaAllocator& allocator,
                          const vk::Device& device,
                          const vk::CommandPool& commandPool,
                          const vk::Queue& transferQueue,
                          const vk::Image& image,
                          unsigned char* srcData,
                          vk::DeviceSize size,
                          const vk::Extent3D& extent);

  void destroy(const VmaAllocator& allocator);

  vk::DeviceAddress getDeviceAddress(const vk::Device& device) const;
};
