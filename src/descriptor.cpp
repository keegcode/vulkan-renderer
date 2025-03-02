#include "descriptor.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "buffer.hpp"
#include "utils.hpp"
#include "vk_mem_alloc.h"

Descriptor::Descriptor() {};

Descriptor::Descriptor(const vk::detail::DispatchLoaderDynamic& dld,
                       const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
                           descriptorBufferProperties,
                       const vk::Device& device,
                       const vk::DescriptorSetLayout& l,
                       vk::DescriptorType t,
                       const VmaAllocator& allocator,
                       const vk::Flags<vk::BufferUsageFlagBits> usage)
    : layout{l}, type{t} {
  vk::DeviceSize setLayoutSize =
      device.getDescriptorSetLayoutSizeEXT(layout, dld);

  alignedSize = utils::getAlignedSize(
      setLayoutSize,
      descriptorBufferProperties.descriptorBufferOffsetAlignment);

  offset = device.getDescriptorSetLayoutBindingOffsetEXT(layout, 0, dld);

  buffer = Buffer{allocator, alignedSize, usage, VMA_MEMORY_USAGE_CPU_TO_GPU,
                  VMA_ALLOCATION_CREATE_MAPPED_BIT};

  vk::DeviceAddress deviceAddres = buffer.getDeviceAddress(device);

  address.setDeviceAddress(deviceAddres);
}

void Descriptor::setUniformBuffer(
    const vk::detail::DispatchLoaderDynamic& dld,
    const vk::Device& device,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
        descriptorBufferProperties,
    const Buffer& uniformBuffer) {
  vk::DescriptorAddressInfoEXT addressInfo =
      vk::DescriptorAddressInfoEXT{}
          .setRange(uniformBuffer.size)
          .setFormat(vk::Format::eUndefined)
          .setAddress(uniformBuffer.getDeviceAddress(device));

  vk::DescriptorDataEXT descriptorData =
      vk::DescriptorDataEXT{}.setPUniformBuffer(&addressInfo);

  vk::DescriptorGetInfoEXT descriptorGetInfo =
      vk::DescriptorGetInfoEXT{}.setType(type).setData(descriptorData);

  device.getDescriptorEXT(
      descriptorGetInfo, descriptorBufferProperties.uniformBufferDescriptorSize,
      buffer.allocationInfo.pMappedData, dld);
}

void Descriptor::setCombinedImageSampler(
    const vk::detail::DispatchLoaderDynamic& dld,
    const vk::Device& device,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
        descriptorBufferProperties,
    const vk::ImageView& imageView,
    const vk::ImageLayout& imageLayout,
    const vk::Sampler& sampler) {
  vk::DescriptorImageInfo imageInfo =
      getDescriptorImageInfo(sampler, imageView, imageLayout);

  vk::DescriptorDataEXT descriptorData =
      vk::DescriptorDataEXT{}.setPCombinedImageSampler(&imageInfo);

  vk::DescriptorGetInfoEXT descriptorGetInfo =
      vk::DescriptorGetInfoEXT{}.setType(type).setData(descriptorData);

  device.getDescriptorEXT(
      descriptorGetInfo,
      descriptorBufferProperties.combinedImageSamplerDescriptorSize,
      ((char*)buffer.allocationInfo.pMappedData) + offset, dld);
}

void Descriptor::destroy(const VmaAllocator& allocator) {
  buffer.destroy(allocator);
}

vk::DescriptorImageInfo Descriptor::getDescriptorImageInfo(
    const vk::Sampler& sampler,
    const vk::ImageView& imageView,
    const vk::ImageLayout& imageLayout) {
  return vk::DescriptorImageInfo{}
      .setSampler(sampler)
      .setImageView(imageView)
      .setImageLayout(imageLayout);
}
