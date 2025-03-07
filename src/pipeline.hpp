#pragma once

#include "shader.hpp"

class Pipeline {
 public:
  Shader vertexShader;
  Shader fragmentShader;

  vk::Pipeline graphicsPipeline;
  vk::PipelineLayout pipelineLayout;

  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;

  std::vector<vk::VertexInputBindingDescription> inputBindings;
  std::vector<vk::VertexInputAttributeDescription> inputAttributes;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  Pipeline();

  Pipeline(const Shader& vert,
           const Shader& frag,
           const vk::Device& device,
           const vk::Viewport& viewport,
           const vk::Rect2D& scissors,
           const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);

  void destroy(const vk::Device& device);

 private:
  void createVertexInputState();
  void createPipeline(const vk::Device& device);
};
