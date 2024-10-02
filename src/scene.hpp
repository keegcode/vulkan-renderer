#pragma once

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

struct Projection {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 perspective;
};

enum class CameraMode {
  Fixed,
  Move
};

struct Camera {
  glm::vec3 pos = glm::vec3{0.0, 0.0, 1.0};
  glm::vec3 up = glm::vec3{0.0, 1.0, 0.0};
  glm::vec3 front = glm::vec3{0.0, 0.0, -1.0};
  glm::vec3 right = glm::normalize(glm::cross(this->front, this->up));

  float pitch = 0.0f;
  float yaw = -90.0f;

  CameraMode mode = CameraMode::Fixed;

  float velocity = 0.1f;
  float sensitivity = 0.1f;
};

struct Object {
  alignas(16) glm::mat4 translation = glm::mat4{1.0f};
  alignas(16) glm::mat4 rotation = glm::mat4{1.0f};
  alignas(16) uint32_t textureIdx;
  alignas(16) uint32_t meshIdx;
};

