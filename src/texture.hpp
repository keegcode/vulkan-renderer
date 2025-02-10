#pragma once

#include "descriptor.hpp"
#include "image.hpp"

#include <vulkan/vulkan_handles.hpp>

class Texture {
public:
  Image image;
  Descriptor descriptor;
  uint32_t swapchainImageCount;

  Texture(const vk::Sampler &sampler, const vk::Device &device,
          const Image &image,
          const vk::DescriptorSetLayout &textureDescriptorSetLayout);

  void destroy(const VmaAllocator &allocator, const vk::Device &device);

private:
  void createSampler();
};
