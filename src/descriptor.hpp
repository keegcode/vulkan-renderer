#pragma once

#include <vulkan/vulkan.hpp>
#include "buffer.hpp"

struct Descriptor {
  Buffer buffer;
  vk::DescriptorSetLayout layout;
  vk::DeviceSize layoutSize;
  vk::DeviceSize offset;
  vk::DeviceOrHostAddressConstKHR address;
  vk::DescriptorType type;

  void destroy(const VmaAllocator& allocator);
};
