#pragma once

#include <string_view>
#include <vulkan/vulkan.hpp>

#include "vk-structs.hpp"

class VertexShader {
public:
  vk::Device device;
  vk::ShaderModule module;
  std::vector<vk::VertexInputBindingDescription> inputBindings;
  std::vector<vk::VertexInputAttributeDescription> inputAttributes;

  void init(vk::Device device, const std::string_view path);
  void destroy();
private:
  void createVertexInputState();
  void createShaderModule(const std::string_view path);
};

class FragmentShader {
public:
  vk::Device device;
  vk::ShaderModule module;
  vk::DescriptorSetLayout descriptorSetLayout;
  vk::DescriptorPool descriptorPool;
  std::vector<vk::DescriptorSet> descriptorSets;
  vk::Sampler sampler;

  void init(vk::Device device, const std::string_view path, const uint16_t swapchainFramesCount, const Buffer& transform, const Image& texture);
  void destroy();
private:
  void createShaderModule(const std::string_view path);
  void createSampler();
  void createDescriptors(uint32_t swapchainFramesCount);
  void updateDescriptors(const Buffer& transform, const Image& texture);
};
