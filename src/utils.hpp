#pragma once

#include <filesystem>
#include <vulkan/vulkan.hpp>

namespace utils {
vk::DeviceSize getAlignedSize(const vk::DeviceSize size,
                              const vk::DeviceSize alignment);
std::vector<char> readFile(const std::filesystem::path& path);
uint32_t getMipLevels(const uint32_t h, const uint32_t w);
}  // namespace utils
