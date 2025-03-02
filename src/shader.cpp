#include "shader.hpp"
#include "utils.hpp"

Shader::Shader() {}

Shader::Shader(const vk::Device& device, const std::string_view path) {
  createShaderModule(device, path);
}

void Shader::destroy(const vk::Device& device) {
  device.destroyShaderModule(module);
}

void Shader::createShaderModule(const vk::Device& device,
                                const std::string_view path) {
  std::vector<char> file = utils::readFile(path);

  vk::ShaderModuleCreateInfo createInfo =
      vk::ShaderModuleCreateInfo{}
          .setPCode(reinterpret_cast<uint32_t*>(file.data()))
          .setCodeSize(file.size());

  module = device.createShaderModule(createInfo, VK_NULL_HANDLE);
}
