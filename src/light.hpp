#pragma once

#include "buffer.hpp"
#include "descriptor.hpp"
#include "vk_mem_alloc.h"
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct LightProperties {
  alignas(16) glm::vec3 pos;
  alignas(16) glm::vec3 color;
  alignas(4) float ambient;
};

class Light {
public:
  LightProperties properties;
  Descriptor descriptor;

  Light();

  Light(const VmaAllocator &allocator, const vk::Device &device,
        const vk::DescriptorSetLayout &descriptorSetLayout);

  void destroy(const VmaAllocator &allocator);

private:
  void createDescriptors(const vk::DescriptorSetLayout &descriptorSetLayout,
                         const VmaAllocator &allocator,
                         const vk::Device &device);
};
