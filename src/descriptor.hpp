#pragma once

#include <vulkan/vulkan.hpp>
#include "buffer.hpp"

class Descriptor {
 public:
  Buffer buffer;
  vk::DescriptorSetLayout layout;
  vk::DeviceSize alignedSize;
  vk::DeviceSize offset;
  vk::DeviceOrHostAddressConstKHR address;
  vk::DescriptorType type;

  Descriptor();

  Descriptor(const vk::detail::DispatchLoaderDynamic& dld,
             const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                 descriptorBufferProperties,
             const vk::Device& device,
             const vk::DescriptorSetLayout& l,
             vk::DescriptorType type,
             const VmaAllocator& allocator,
             const vk::Flags<vk::BufferUsageFlagBits> usage);

  vk::DescriptorImageInfo getDescriptorImageInfo(
      const vk::Sampler& sampler,
      const vk::ImageView& imageView,
      const vk::ImageLayout& imageLayout);

  void setUniformBuffer(const vk::detail::DispatchLoaderDynamic& dld,
                        const vk::Device& device,
                        const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                            descriptorBufferProperties,
                        const Buffer& uniformBuffer);

  void setCombinedImageSampler(
      const vk::detail::DispatchLoaderDynamic& dld,
      const vk::Device& device,
      const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
          descriptorBufferProperties,
      const vk::ImageView& imageView,
      const vk::ImageLayout& imageLayout,
      const vk::Sampler& sampler);

  void destroy(const VmaAllocator& allocator);
};
