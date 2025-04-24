#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <assimp/material.h>
#include <cstdint>
#include <assimp/scene.h>
#include <filesystem>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "display.hpp"
#include "gpu.hpp"

const std::array<std::string, 6> CUBEMAP_FILES{"right",  "left",  "top",
                                               "bottom", "front", "back"};

struct SpotLight {
  glm::vec3 direction;
  float constant;
  glm::vec3 position;
  float linear;
  glm::vec3 ambient;
  float quadratic;
  glm::vec3 diffuse;
  float cutOff;
  glm::vec3 specular;
  float outerCutOff;
  Buffer uniform;
};

struct PointLight {
  glm::vec3 position;
  float constant;
  glm::vec3 ambient;
  float linear;
  glm::vec3 diffuse;
  float quadratic;
  alignas(16) glm::vec3 specular;
  Buffer uniform;
};

struct DirectionalLight {
  alignas(16) glm::vec3 direction;
  alignas(16) glm::vec3 ambient;
  alignas(16) glm::vec3 diffuse;
  alignas(16) glm::vec3 specular;
  Buffer uniform;
  glm::vec3 position;
};

enum class CameraMode { Fixed, Move };

struct Camera {
  glm::vec3 position = glm::vec3{0.0, 0.0, 0.0};
  glm::vec3 up = glm::vec3{0.0, 1.0, 0.0};
  glm::vec3 front = glm::vec3{0.0, 0.0, -1.0};
  glm::vec3 right = glm::normalize(glm::cross(this->front, this->up));

  float pitch = 0.0f;
  float yaw = -90.0f;

  CameraMode mode = CameraMode::Fixed;

  float velocity = 0.1f;
  float sensitivity = 0.1f;
};

struct Mesh {
  Buffer vertexBuffer;
  Buffer indexBuffer;
  uint32_t materialIdx;
  uint32_t indicesCount;
};

enum class AlphaMode { Opaque, Blend, Mask };

struct Material {
  glm::vec3 specular = glm::vec3{1.0};
  float shininess = 32.0;
  glm::vec3 emissive = glm::vec3{0.0};
  float alphaCutoff = 0.001;
  glm::vec3 color = glm::vec3{1.0};
  float transmissionFactor = 0.0f;
  float rougness = 1.0f;
  Buffer uniform;
  uint32_t diffuseTextureIdx = 0;
  uint32_t specularTextureIdx = 0;
  vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eBack;
  AlphaMode alphaMode = AlphaMode::Opaque;
};

enum class TextureType {
  BaseColor,
  Specular,
  Cube,
};

struct Texture {
  Image image;
  TextureType type;
  vk::Sampler sampler;
};

struct Asset {
  std::vector<uint32_t> meshes;
  std::filesystem::path path;
};

struct Projection {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 perspective;
};

struct Entity {
  glm::mat4 matrix = glm::mat4{1.0f};
  uint32_t assetIdx = 0;
  Buffer uniform;
};

struct EngineConfig {
  Projection projection;
  DirectionalLight directionalLight;
  std::vector<PointLight> pointLights;
  std::vector<SpotLight> spotLights;
  std::vector<Entity> entities;
  std::vector<std::filesystem::path> assets;
};

enum class GLTFMagFilter { Nearest = 9728, Linear = 9729 };

enum class GLTFMinFilter {
  Nearest = 9728,
  Linear = 9729,
  NearestMipmapNearest = 9984,
  LinearMipmapNearest = 9985,
  NearestMipmapLinear = 9986,
  LinearMipmapLinear = 9987
};

struct ImageData {
  int width, height;
  uint8_t* data;
};

class Engine {
 public:
  Pipeline pipeline;
  Pipeline skyboxPipeline;

  Texture skybox;
  Texture reflectionCube;

  Projection projection;

  std::vector<Entity> entities;
  std::vector<Material> materials;
  std::vector<Texture> textures;
  std::vector<Asset> assets;
  std::vector<Mesh> meshes;

  DirectionalLight directionalLight;
  std::vector<PointLight> pointLights;
  std::vector<SpotLight> spotLights;

  Camera camera;

  Descriptor entitiesDescriptor;
  Descriptor materialsDescriptor;
  Descriptor texturesDescriptor;
  Descriptor lightsDescriptor;
  Descriptor skyboxDescriptor;
  Descriptor reflectionCubeDescriptor;

  bool isRunning = true;

  Engine(const Display& display, const GPU& gpu);

  void init(const EngineConfig& state);

  void drawFrame(float deltaTime);
  void drawSkybox(const FrameData& data);
  void processInput(float deltaTime);
  void destroyTexture(const Texture& texture);
  void destroy();

 private:
  Display display;
  GPU gpu;

  bool shouldBeResized = false;

  void createPipeline();
  void loadStatic();
  void loadConfig(const EngineConfig& state);

  void loadAsset(const std::filesystem::path& path);
  void processNode(Asset& asset, const aiScene* scene, const aiNode* node, std::vector<std::pair<uint32_t, std::string>>& tasks);
  void loadMesh(Asset& asset, const aiScene* scene, const aiMesh* mesh, std::vector<std::pair<uint32_t, std::string>>& tasks);
  void loadMaterial(Asset& asset, Mesh& mesh, const aiMaterial* assimpMaterial, std::vector<std::pair<uint32_t, std::string>>& tasks);
  vk::SamplerCreateInfo extractGLTFSampler(const aiTextureMapMode u,
                                           const aiTextureMapMode v,
                                           const GLTFMagFilter mag,
                                           const GLTFMinFilter min,
                                           const Image& image) const;

  Image loadImage(const std::filesystem::path& path);
  Image loadCubemap(const std::string& type);
  void loadSkybox();
  void loadReflectionCube();
  void drawEntities(const FrameData& frameData);
};
