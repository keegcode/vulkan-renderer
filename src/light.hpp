#pragma once

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include "buffer.hpp"
#include "vk_mem_alloc.h"

struct LightProperties {
  alignas(16) glm::vec3 pos;
  alignas(16) glm::vec3 color;
  alignas(4) float ambient;
};

class Light {
public:
  LightProperties properties;

  Buffer ubo;

  vk::DescriptorSetLayout descriptorSetLayout;
  std::vector<vk::DescriptorSet> descriptorSets;

  Light();

  Light(
    const VmaAllocator& allocator,
    const vk::Device& device, 
    const uint32_t swapchainImageCount,
    const vk::DescriptorPool& descriptorPool,
    const vk::DescriptorSetLayout& descriptorSetLayout
  );

  void destroy(const VmaAllocator& allocator);
private:
  void createDescriptors(const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const VmaAllocator& allocator, const vk::Device& device, const uint32_t swapchainImageCount);
};
