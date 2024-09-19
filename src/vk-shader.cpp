#include <vulkan/vulkan.hpp>
#include "vk-shader.hpp"
#include "fs.hpp"

void VertexShader::init(vk::Device d, const std::string_view path) {
  device = d;
  createShaderModule(path);
  createVertexInputState();
}

void VertexShader::destroy() {
  device.destroyShaderModule(module);
}

void VertexShader::createShaderModule(const std::string_view path) {
  std::vector<char> file = fs::readFile(path);

  vk::ShaderModuleCreateInfo createInfo = vk::ShaderModuleCreateInfo{}
    .setPCode(reinterpret_cast<uint32_t*>(file.data()))
    .setCodeSize(file.size());
  
  module = device.createShaderModule(createInfo, VK_NULL_HANDLE);
}

void VertexShader::createVertexInputState() {
  vk::VertexInputBindingDescription vertexBinding = vk::VertexInputBindingDescription{}
    .setStride(sizeof(Vertex))
    .setInputRate(vk::VertexInputRate::eVertex)
    .setBinding(0);

  inputBindings = {vertexBinding}; 

  vk::VertexInputAttributeDescription posAttribute = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(0)
    .setOffset(offsetof(Vertex, pos))
    .setFormat(vk::Format::eR32G32B32Sfloat);

  vk::VertexInputAttributeDescription clrAttribute = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(1)
    .setOffset(offsetof(Vertex, clr))
    .setFormat(vk::Format::eR32G32B32Sfloat);

  vk::VertexInputAttributeDescription uvAttribute = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(2)
    .setOffset(offsetof(Vertex, uv))
    .setFormat(vk::Format::eR32G32Sfloat);

  inputAttributes = {posAttribute, clrAttribute, uvAttribute};
}

void FragmentShader::createShaderModule(const std::string_view path) {
  std::vector<char> file = fs::readFile(path);

  vk::ShaderModuleCreateInfo createInfo = vk::ShaderModuleCreateInfo{}
    .setPCode(reinterpret_cast<uint32_t*>(file.data()))
    .setCodeSize(file.size());
  
  module = device.createShaderModule(createInfo, VK_NULL_HANDLE);
}

void FragmentShader::createSampler() {
  vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo{}
    .setMagFilter(vk::Filter::eLinear)
    .setMinFilter(vk::Filter::eLinear)
    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
    .setAnisotropyEnable(0)
    .setCompareEnable(0);

  sampler = device.createSampler(samplerCreateInfo);
}

void FragmentShader::createDescriptors(uint32_t swapchainFramesCount) {
  vk::DescriptorPoolSize samplerPool = vk::DescriptorPoolSize{}
    .setDescriptorCount(swapchainFramesCount)
    .setType(vk::DescriptorType::eCombinedImageSampler);

  vk::DescriptorPoolSize uniformPool = vk::DescriptorPoolSize{}
    .setDescriptorCount(swapchainFramesCount)
    .setType(vk::DescriptorType::eUniformBuffer);

  std::vector<vk::DescriptorPoolSize> poolSizes{
    samplerPool,
    uniformPool
  };

  vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{}
    .setMaxSets(swapchainFramesCount)
    .setPoolSizes(poolSizes)
    .setPoolSizeCount(poolSizes.size());

  descriptorPool = device.createDescriptorPool(descriptorPoolCreateInfo, nullptr);

  vk::DescriptorSetLayoutBinding samplerBinding = vk::DescriptorSetLayoutBinding{}
    .setBinding(0)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eFragment)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
    .setImmutableSamplers(sampler);

  vk::DescriptorSetLayoutBinding uniformBinding = vk::DescriptorSetLayoutBinding{}
    .setBinding(1)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eVertex)
    .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{
    samplerBinding,
    uniformBinding
  };

  vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{}
    .setBindings(setLayoutBindings)
    .setBindingCount(setLayoutBindings.size());

  descriptorSetLayout = device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo, nullptr);

  std::vector<vk::DescriptorSetLayout> layouts(swapchainFramesCount, descriptorSetLayout); 

  vk::DescriptorSetAllocateInfo allocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorPool(descriptorPool)
    .setDescriptorSetCount(layouts.size())
    .setSetLayouts(layouts);

  descriptorSets = device.allocateDescriptorSets(allocateInfo);
}

void FragmentShader::updateDescriptors(const Buffer& transform, const Image& texture) {
  for (size_t i = 0; i < descriptorSets.size(); i++) {
    vk::DescriptorImageInfo imageInfo = vk::DescriptorImageInfo{}
      .setSampler(sampler) 
      .setImageView(texture.imageView)
      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::DescriptorBufferInfo bufferInfo = vk::DescriptorBufferInfo{}
      .setBuffer(transform.buffer)
      .setRange(sizeof(Transform))
      .setOffset(0);

    vk::WriteDescriptorSet samplerWrite = vk::WriteDescriptorSet{}
      .setDstSet(descriptorSets[i])
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setImageInfo(imageInfo);
    
    vk::WriteDescriptorSet bufferWrite = vk::WriteDescriptorSet{}
      .setDstSet(descriptorSets[i])
      .setDstBinding(1)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setBufferInfo(bufferInfo);

    std::vector<vk::WriteDescriptorSet> writes{
      samplerWrite,
      bufferWrite,
    };

    device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
  }
}

void FragmentShader::init(vk::Device d, const std::string_view path, const uint16_t swapchainFramesCount, const Buffer& transform, const Image& texture) {
  device = d;
  createShaderModule(path);
  createSampler();
  createDescriptors(swapchainFramesCount);
  updateDescriptors(transform, texture);
}

void FragmentShader::destroy() {
  device.destroyDescriptorSetLayout(descriptorSetLayout); 
  device.destroySampler(sampler);
  device.destroyDescriptorPool(descriptorPool); 
  device.destroyShaderModule(module);
}
