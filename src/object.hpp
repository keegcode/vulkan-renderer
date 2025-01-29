#pragma once

#include "buffer.hpp"
#include "vk_mem_alloc.h"
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct UniformBuffer {
  alignas(16) glm::mat4 translation = glm::mat4{1.0f};
  alignas(16) glm::mat4 rotation = glm::mat4{1.0f};
  alignas(16) glm::mat4 scale = glm::mat4{1.0};
  alignas(16) glm::vec3 color = glm::vec3{0.5};
};

class Object {
public:
  UniformBuffer uniform;

  uint32_t textureIdx = 0;
  uint32_t meshIdx = 0;
  uint32_t pipelineIdx = 0;

  Buffer ubo;

  vk::DescriptorSetLayout descriptorSetLayout;
  std::vector<vk::DescriptorSet> descriptorSets;

  Object();

  Object(const VmaAllocator &allocator, const vk::Device &device,
         const uint32_t swapchainImageCount,
         const vk::DescriptorPool &descriptorPool,
         const vk::DescriptorSetLayout &descriptorSetLayout);

  void destroy(const VmaAllocator &allocator);

private:
  void createDescriptors(const vk::DescriptorPool &descriptorPool,
                         const vk::DescriptorSetLayout &descriptorSetLayout,
                         const VmaAllocator &allocator,
                         const vk::Device &device,
                         const uint32_t swapchainImageCount);
};
