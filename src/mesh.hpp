#pragma once

#include "buffer.hpp"

struct Vertex {
  float pos[3];
  float clr[3];
  float uv[2];
  float normals[3];
};

class Mesh {
public:
  Buffer vertexBuffer;
  Buffer indexBuffer;
  uint32_t indicesCount;

  Mesh();
  Mesh(const VmaAllocator& allocator, const std::string_view path);

  void destroy(const VmaAllocator& allocator);
};
