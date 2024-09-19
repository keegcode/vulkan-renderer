#include <fstream>
#include <string_view>
#include <vector>
#include "fs.hpp"

std::vector<char> fs::readFile(const std::string_view path) {
  std::ifstream file{path.data(), std::ios::in | std::ios::binary | std::ios::ate};
  std::ifstream::pos_type size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> bytes(size);
  file.read(bytes.data(), size);

  return bytes;
};
