#include "texture.hpp"
#include "image.hpp"

Texture::Texture(
  const vk::Sampler& sampler, 
  const vk::Device& device, 
  const vk::DescriptorPool& descriptorPool, 
  const Image& i, 
  const uint32_t swapImgCount, 
  const vk::DescriptorSetLayout& textureDescriptorSetLayout
): image{i}, swapchainImageCount{swapImgCount} {
  std::vector<vk::DescriptorSetLayout> layouts(swapchainImageCount, textureDescriptorSetLayout); 

  vk::DescriptorSetAllocateInfo allocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorPool(descriptorPool)
    .setDescriptorSetCount(layouts.size())
    .setSetLayouts(layouts);

  descriptorSets = device.allocateDescriptorSets(allocateInfo);

  for (size_t i = 0; i < descriptorSets.size(); i++) {
    vk::DescriptorImageInfo imageInfo = vk::DescriptorImageInfo{}
      .setSampler(sampler) 
      .setImageView(image.view)
      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::WriteDescriptorSet samplerWrite = vk::WriteDescriptorSet{}
      .setDstSet(descriptorSets[i])
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setImageInfo(imageInfo);
    
    std::vector<vk::WriteDescriptorSet> writes{
      samplerWrite,
    };

    device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
  }
}

void Texture::destroy(const VmaAllocator& allocator, const vk::Device& device) {
  image.destroy(allocator, device);
}

