#pragma once

#include "image.hpp"
#include <vulkan/vulkan_handles.hpp>

class Texture {
public:
  Image image;
  std::vector<vk::DescriptorSet> descriptorSets;
  uint32_t swapchainImageCount;

  Texture(const vk::Sampler& sampler, const vk::Device& device, const vk::DescriptorPool& descriptorPool, const Image& image, const uint32_t swapchainImageCount, const vk::DescriptorSetLayout& textureDescriptorSetLayout);
  
  void destroy(const vk::DescriptorPool& descriptorPool, const vk::Device& device);
private:
  void createSampler();
};
