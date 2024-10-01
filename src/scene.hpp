#pragma once

#include <glm/glm.hpp>

struct Projection {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 perspective;
};

struct Camera {
  glm::vec3 pos = glm::vec3{0.0, 0.0, 0.0};
  glm::vec3 up = glm::vec3{0.0, 1.0, 0.0};
  glm::vec3 front = glm::vec3{0.0, 0.0, -1.0};

  float velocity = 2.0f;
  float sensitivity = 0.5f;
};

struct Object {
  glm::vec3 pos;
  uint32_t textureIdx;
  uint32_t meshIdx;
};

