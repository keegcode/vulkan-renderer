#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "buffer.hpp"
#include "image.hpp"

struct Descriptor {
  Buffer buffer;
  vk::DescriptorSetLayout layout;
  vk::DeviceSize layoutSize;
  vk::DeviceSize offset;
  vk::DeviceOrHostAddressConstKHR address;
  vk::DescriptorType type;

  static Descriptor createUniformDescriptor(
      const uint32_t count,
      const vk::DescriptorSetLayout& layout,
      const vk::Device device,
      const VmaAllocator& allocator,
      const vk::detail::DispatchLoaderDynamic& dld,
      const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
          descriptorBufferProperties);

  static Descriptor createTextureDescriptor(
      const uint32_t count,
      const vk::DescriptorSetLayout& layout,
      const vk::Device device,
      const VmaAllocator& allocator,
      const vk::detail::DispatchLoaderDynamic& dld,
      const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
          descriptorBufferProperties);

  void setUniformBuffer(const Buffer& src,
                        uint32_t offset,
                        const vk::Device& device,
                        const vk::detail::DispatchLoaderDynamic& dld,
                        const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                            descriptorBufferProperties);

  void setImage(const Image& src, const vk::Sampler& sampler,
                        uint32_t offset,
                        const vk::Device& device,
                        const vk::detail::DispatchLoaderDynamic& dld,
                        const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                            descriptorBufferProperties);

  void destroy(const VmaAllocator& allocator);
};
