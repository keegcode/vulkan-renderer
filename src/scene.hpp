#pragma once

#include <glm/mat4x4.hpp>
#include "descriptor.hpp"
#include "image.hpp"

struct ProjectionProperties {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 perspective;
};

struct ObjectProperties {
  alignas(16) glm::mat4 translation = glm::mat4{1.0f};
  alignas(16) glm::mat4 rotation = glm::mat4{1.0f};
  alignas(16) glm::mat4 scale = glm::mat4{1.0};
  alignas(16) glm::vec3 color = glm::vec3{0.5};
};

struct MaterialProperties {
  alignas(16) glm::vec3 ambient;
  alignas(16) glm::vec3 diffuse;
  alignas(16) glm::vec3 specular;
  alignas(16) float shininess;
  alignas(16) bool solid;
};

struct LightProperties {
  alignas(16) glm::vec3 pos;
  alignas(16) glm::vec3 color;
  alignas(4) float ambient;
};

class Projection {
 public:
  ProjectionProperties properties;
  Descriptor descriptor;
  Buffer buffer;

  Projection();
  Projection(const ProjectionProperties& properties,
             const Descriptor& descriptor,
             const Buffer& buffer);
};

class Object {
 public:
  ObjectProperties properties;
  Descriptor descriptor;
  Buffer buffer;

  uint32_t textureIdx = 0;
  uint32_t meshIdx = 0;
  uint32_t materialIdx = 0;

  Object();
  Object(const ObjectProperties& properties,
         const Descriptor& descriptor,
         const Buffer& buffer);

  void destroy(const VmaAllocator& allocator);
};

class Material {
 public:
  MaterialProperties properties;
  Descriptor descriptor;
  Buffer buffer;

  Material();
  Material(const MaterialProperties& properties,
           const Descriptor& descriptor,
           const Buffer& buffer);

  void destroy(const VmaAllocator& allocator);
};

class Texture {
 public:
  Image image;
  Descriptor descriptor;
  vk::Sampler sampler;

  Texture(const Image& image,
          const Descriptor& descriptor,
          const vk::Sampler& sampler);

  void destroy(const VmaAllocator& allocator, const vk::Device& device);
};

class Light {
 public:
  LightProperties properties;
  Descriptor descriptor;
  Buffer buffer;

  Light();
  Light(const LightProperties& lightProperties,
        const Descriptor& descriptor,
        const Buffer& buffer);

  void destroy(const VmaAllocator& allocator);
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
