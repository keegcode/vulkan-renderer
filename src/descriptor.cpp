#include "descriptor.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "utils.hpp"

void Descriptor::destroy(const VmaAllocator& allocator) {
  buffer.destroy(allocator);
}

Descriptor Descriptor::createUniformDescriptor(
    uint32_t count,
    const vk::DescriptorSetLayout& layout,
    const vk::Device device,
    const VmaAllocator& allocator,
    const vk::detail::DispatchLoaderDynamic& dld,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
        descriptorBufferProperties) {
  Descriptor descriptor{};
  descriptor.type = vk::DescriptorType::eUniformBuffer;

  descriptor.layoutSize = utils::getAlignedSize(
      device.getDescriptorSetLayoutSizeEXT(layout, dld),
      descriptorBufferProperties.descriptorBufferOffsetAlignment);

  descriptor.offset =
      device.getDescriptorSetLayoutBindingOffsetEXT(layout, 0, dld);

  descriptor.buffer =
      Buffer{allocator, descriptor.layoutSize * static_cast<uint32_t>(count),
             vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress,
             VMA_MEMORY_USAGE_AUTO,
             VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

  descriptor.address.setDeviceAddress(
      descriptor.buffer.getDeviceAddress(device));

  return descriptor;
};

Descriptor Descriptor::createTextureDescriptor(
    uint32_t count,
    const vk::DescriptorSetLayout& layout,
    const vk::Device device,
    const VmaAllocator& allocator,
    const vk::detail::DispatchLoaderDynamic& dld,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
        descriptorBufferProperties) {
  Descriptor descriptor{};
  descriptor.type = vk::DescriptorType::eCombinedImageSampler;

  descriptor.layoutSize = utils::getAlignedSize(
      device.getDescriptorSetLayoutSizeEXT(layout, dld),
      descriptorBufferProperties.descriptorBufferOffsetAlignment);

  descriptor.offset =
      device.getDescriptorSetLayoutBindingOffsetEXT(layout, 0, dld);

  descriptor.buffer =
      Buffer{allocator, descriptor.layoutSize * static_cast<uint32_t>(count),
             vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                 vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT | 
                 vk::BufferUsageFlagBits::eShaderDeviceAddress,
             VMA_MEMORY_USAGE_AUTO,
             VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

  descriptor.address.setDeviceAddress(
      descriptor.buffer.getDeviceAddress(device));

  return descriptor;
};

void Descriptor::setUniformBuffer(
    const Buffer& src,
    uint32_t index,
    const vk::Device& device,
    const vk::detail::DispatchLoaderDynamic& dld,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
        descriptorBufferProperties) {
  char* uniformDescriptorPtr =
      reinterpret_cast<char*>(buffer.allocationInfo.pMappedData);

  vk::DescriptorAddressInfoEXT uniformDescriptorAddressInfo =
      vk::DescriptorAddressInfoEXT{}
          .setRange(src.size)
          .setFormat(vk::Format::eUndefined)
          .setAddress(src.getDeviceAddress(device));

  vk::DescriptorGetInfoEXT uniformDescriptorInfo =
      vk::DescriptorGetInfoEXT{}
          .setData(vk::DescriptorDataEXT{}.setPUniformBuffer(
              &uniformDescriptorAddressInfo))
          .setType(type);

  device.getDescriptorEXT(
      uniformDescriptorInfo,
      descriptorBufferProperties.uniformBufferDescriptorSize,
      uniformDescriptorPtr + (index * layoutSize) + offset, dld);
}

void Descriptor::setImage(
    const Image& src,
    const vk::Sampler& sampler,
    uint32_t index,
    const vk::Device& device,
    const vk::detail::DispatchLoaderDynamic& dld,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT&
        descriptorBufferProperties) {
  char* textureDescriptorPtr =
      reinterpret_cast<char*>(buffer.allocationInfo.pMappedData);

  vk::DescriptorImageInfo textureProjectionDescriptorImageInfo =
      vk::DescriptorImageInfo{}
        .setSampler(sampler)
        .setImageView(src.view)
        .setImageLayout(src.layout);
        
  vk::DescriptorGetInfoEXT textureDescriptorInfo =
      vk::DescriptorGetInfoEXT{}
          .setData(vk::DescriptorDataEXT{}.setPCombinedImageSampler(
              &textureProjectionDescriptorImageInfo))
          .setType(type);

  device.getDescriptorEXT(
      textureDescriptorInfo,
      descriptorBufferProperties.combinedImageSamplerDescriptorSize,
      textureDescriptorPtr + (index * layoutSize) + offset, dld);
}
