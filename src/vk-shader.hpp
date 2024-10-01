#pragma once

#include <string_view>
#include <vulkan/vulkan.hpp>

class Shader {
public:
  vk::ShaderModule module;

  Shader(const vk::Device& device, const std::string_view path);
  void destroy(const vk::Device& device);
private:
  void createShaderModule(const vk::Device& device, const std::string_view path);
};

