#pragma once

#include <vulkan/vulkan.hpp>

namespace utils {
vk::DeviceSize getAlignedSize(const vk::DeviceSize size,
                              const vk::DeviceSize alignment);
std::vector<char> readFile(const std::string_view path);
}  // namespace utils
