#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "vk_mem_alloc.h"

struct Image {
  vk::Image image;
  vk::ImageView imageView;
  vk::Extent2D extent;
  VmaAllocation allocation;
};

struct Buffer {
  vk::Buffer buffer;
  VmaAllocation allocation;
};

struct Transform {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct Vertex {
  float pos[3];
  float clr[3];
  float uv[2];
};

struct Mesh {
  Image texture;
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
  Buffer vertexBuffer;
  Buffer indexBuffer;
};

struct Entity {
  size_t meshIndex;
  glm::vec3 pos;
};
