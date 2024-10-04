#include "object.hpp"

Object::Object() {
}

Object::Object(const VmaAllocator& allocator, const vk::Device& device, const uint32_t swapchainImageCount, const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout) {
  createDescriptors(descriptorPool, descriptorSetLayout, allocator, device, swapchainImageCount);
}

void Object::createDescriptors(const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const VmaAllocator& allocator, const vk::Device& device, const uint32_t swapchainImageCount) {
  std::vector<vk::DescriptorSetLayout> layouts(swapchainImageCount, descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorPool(descriptorPool)
    .setDescriptorSetCount(swapchainImageCount)
    .setSetLayouts(layouts);

  descriptorSets = device.allocateDescriptorSets(allocateInfo);

  ubo = Buffer{allocator, sizeof(UniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer};

  for (size_t i = 0; i < descriptorSets.size(); i++) {
    vk::DescriptorBufferInfo uboInfo = vk::DescriptorBufferInfo{}
      .setBuffer(ubo.buffer)
      .setRange(sizeof(UniformBuffer))
      .setOffset(0);
    
    vk::WriteDescriptorSet uboWrite = vk::WriteDescriptorSet{}
      .setDstSet(descriptorSets[i])
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setBufferInfo(uboInfo);

    std::vector<vk::WriteDescriptorSet> writes{
      uboWrite,
    };

    device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
  }
}
