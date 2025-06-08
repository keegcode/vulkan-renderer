#pragma once

#include <filesystem>
#include <vulkan/vulkan.hpp>
#include <glm/mat4x4.hpp>
#include <assimp/matrix4x4.h>

namespace utils {
vk::DeviceSize getAlignedSize(const vk::DeviceSize size,
                              const vk::DeviceSize alignment);
std::vector<char> readFile(const std::filesystem::path& path);
uint32_t getMipLevels(const uint32_t h, const uint32_t w);
std::vector<vk::DescriptorSetLayoutCreateInfo> getDescriptorSetLayoutCreateInfo(
    const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& layout);
glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4* from);
}  // namespace utils
