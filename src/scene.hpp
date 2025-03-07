#pragma once

#include <glm/mat4x4.hpp>
#include "image.hpp"

struct Projection {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 perspective;
};

struct Object {
  alignas(16) glm::mat4 matrix = glm::mat4{1.0f};
  alignas(16) glm::vec3 color = glm::vec3{0.5};
  uint32_t textureIdx = 0;
  uint32_t meshIdx = 0;
  uint32_t materialIdx = 0;
};

struct Material {
  alignas(16) glm::vec3 ambient;
  alignas(16) glm::vec3 diffuse;
  alignas(16) glm::vec3 specular;
  alignas(4) float shininess;
  alignas(4) bool solid;
};

struct Light {
  alignas(16) glm::vec3 pos;
  alignas(16) glm::vec3 color;
  alignas(4) float ambient;
};

struct Texture {
  Image image;
  void destroy(const VmaAllocator& allocator, const vk::Device& device);
};

enum class CameraMode { Fixed, Move };

struct Camera {
  glm::vec3 pos = glm::vec3{0.0, 0.0, 0.0};
  glm::vec3 up = glm::vec3{0.0, 1.0, 0.0};
  glm::vec3 front = glm::vec3{0.0, 0.0, -1.0};
  glm::vec3 right = glm::normalize(glm::cross(this->front, this->up));

  float pitch = 0.0f;
  float yaw = -90.0f;

  CameraMode mode = CameraMode::Fixed;

  float velocity = 0.1f;
  float sensitivity = 0.1f;
};
