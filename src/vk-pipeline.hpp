#pragma once

#include "buffer.hpp"
#include "vk-shader.hpp"

class Pipeline {
public:
  Shader vertexShader;
  Shader fragmentShader;
  
  vk::Pipeline graphicsPipeline;
  vk::PipelineLayout pipelineLayout;

  Buffer projection;

  std::vector<vk::DescriptorSet> descriptorSets;
  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;

  std::vector<vk::VertexInputBindingDescription> inputBindings;
  std::vector<vk::VertexInputAttributeDescription> inputAttributes;

  uint32_t swapchainImageCount; 

  const vk::Viewport viewport;
  const vk::Rect2D scissors;

  Pipeline(
    const VmaAllocator& allocator,
    const Shader& vert, 
    const Shader& frag, 
    const vk::Device& device, 
    const vk::Viewport& viewport, 
    const vk::Rect2D& scissors, 
    const uint32_t swapchainImageCount,
    const vk::DescriptorPool& descriptorPool,
    const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts
  );

  void destroy(const vk::Device& device);
private:
  void createVertexInputState();
  void createDescriptors(const vk::DescriptorPool& descriptorPool, const VmaAllocator& allocator, const vk::Device& device);
  void createPipeline(const vk::Device& device);
};
