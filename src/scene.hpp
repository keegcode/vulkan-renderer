#pragma once

#include <glm/mat4x4.hpp>
#include "buffer.hpp"
#include "descriptor.hpp"
#include "image.hpp"

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

struct ProjectionProperties {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 perspective;
};

struct EntityProperties {
  glm::mat4 matrix = glm::mat4{1.0f};
  alignas(16) glm::vec3 color = glm::vec3{0.5};
};

struct Texture {
  Image image;
  Image diffuseMap;
  Image specularMap;
  void destroy(const VmaAllocator& allocator, const vk::Device& device);
};

struct MaterialProperties {
  glm::vec3 specular;
  float shininess;
  alignas(4) bool solid;
  alignas(4) bool light;
};

struct LightProperties {
  alignas(16) glm::vec3 pos;
  alignas(16) glm::vec3 ambient;
  alignas(16) glm::vec3 diffuse;
  alignas(16) glm::vec3 specular;
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

struct Entity {
  EntityProperties properties;
  Buffer uniform;
  uint32_t textureIdx = 0;
  uint32_t meshIdx = 0;
  uint32_t materialIdx = 0;
};

struct Material {
  MaterialProperties properties;
  Buffer uniform;
};

struct Light {
  Buffer uniform;
  Descriptor descriptor;
  LightProperties properties;
};

struct Projection {
  Buffer uniform;
  Descriptor descriptor;
  ProjectionProperties properties;
};

struct Scene {
  Light light;
  Projection projection;
  Camera camera;

  std::vector<Entity> entities;
  std::vector<Material> materials;
  std::vector<Texture> textures;
  std::vector<Mesh> meshes;

  Descriptor entitiesDescriptor;
  Descriptor materialsDescriptor;
  Descriptor texturesDescriptor;

  void destroy(const VmaAllocator& allocator);
};
