#pragma once

#include <string_view>
#include <vector>

namespace fs {
  std::vector<char> readFile(const std::string_view path);
}
