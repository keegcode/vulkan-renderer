#include "utils.hpp"

#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

vk::DeviceSize utils::getAlignedSize(const vk::DeviceSize size,
                                     const vk::DeviceSize alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

std::vector<char> utils::readFile(const std::string_view path) {
  std::ifstream file{path.data(),
                     std::ios::in | std::ios::binary | std::ios::ate};

  if (!file.is_open()) {
    throw std::runtime_error{std::string{"Failed to open file: "} +
                             path.data()};
  }

  std::ifstream::pos_type size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> bytes(size);
  file.read(bytes.data(), size);

  return bytes;
}
