#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "buffer.hpp"
#include "image.hpp"

struct Descriptor {
  Buffer buffer;
  vk::DescriptorSetLayout layout;
  vk::DeviceSize size;
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
                        uint32_t index,
                        uint32_t binding,
                        const vk::Device& device,
                        const vk::detail::DispatchLoaderDynamic& dld,
                        const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                            descriptorBufferProperties);

  void setImage(const Image& src,
                const vk::Sampler& sampler,
                uint32_t index,
                uint32_t binding,
                const vk::Device& device,
                const vk::detail::DispatchLoaderDynamic& dld,
                const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                    descriptorBufferProperties);

  vk::DeviceSize getOffset(const vk::Device& device,
                           uint32_t binding,
                           const vk::detail::DispatchLoaderDynamic& dld);

  void destroy(const VmaAllocator& allocator);
};
