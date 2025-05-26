#include "utils.hpp"

#include <fstream>
#include <vulkan/vulkan_structs.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

vk::DeviceSize utils::getAlignedSize(const vk::DeviceSize size,
                                     const vk::DeviceSize alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

uint32_t utils::getMipLevels(const uint32_t h, const uint32_t w) {
  return static_cast<uint32_t>(std::floor(log2(std::max(w, h)) + 1));
}

std::vector<char> utils::readFile(const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::in | std::ios::binary | std::ios::ate};

  assert(file.is_open());

  std::ifstream::pos_type size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> bytes(size);
  file.read(bytes.data(), size);

  return bytes;
}

std::pair<std::vector<vk::DescriptorSetLayoutCreateInfo>, std::vector<vk::DescriptorPoolSize>> utils::getDescriptorSetLayoutCreateInfo(
  const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& layout
) {
  std::vector<vk::DescriptorSetLayoutCreateInfo> sets{};
  std::vector<vk::DescriptorPoolSize> sizes{};

  for (const std::vector<vk::DescriptorSetLayoutBinding>& bindings : layout) {
    sets.push_back(
      vk::DescriptorSetLayoutCreateInfo{}
        .setBindings(bindings)
        .setBindingCount(bindings.size())
    );

    for (const vk::DescriptorSetLayoutBinding& binding : bindings) {
      sizes.push_back(vk::DescriptorPoolSize{}
          .setDescriptorCount(1)
          .setType(binding.descriptorType));
    }
  }

  return {sets, sizes};
}
