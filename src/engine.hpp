#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <assimp/material.h>
#include <assimp/scene.h>
#include <cstdint>
#include <filesystem>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "display.hpp"
#include "gpu.hpp"

const std::array<std::string, 6> CUBEMAP_FILES{"right",  "left",  "top",
                                               "bottom", "front", "back"};

struct Transform {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
  Buffer uniform;
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

  float velocity = 7.0f;
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
  glm::vec3 specular = glm::vec3{1.0f};
  float shininess = 32.0f;
  glm::vec3 emissive = glm::vec3{0.0f};
  float alphaCutoff = 0.001f;
  glm::vec3 color = glm::vec3{1.0f};
  float transmissionFactor = 0.0f;
  float roughness = 1.0f;
  uint32_t normalTextureIdx = 1;
  uint32_t diffuseTextureIdx = 0;
  uint32_t specularTextureIdx = 0;
  uint32_t heightTextureIdx = 0;
  vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eBack;
  AlphaMode alphaMode = AlphaMode::Opaque;
};

enum class TextureType { BaseColor, Specular, Cube, Normal, Height, Shadow };

struct Texture {
  Image image;
  TextureType type;
  vk::Sampler sampler;
  std::string path;
};

struct SpotLight {
  glm::vec3 direction;
  float constant;
  glm::vec3 position;
  float linear;
  glm::vec3 ambient;
  glm::vec3 diffuse;
  float cutOff;
  glm::vec3 specular;
  float outerCutOff;
  glm::mat4 lightSpaceMatrix;
  uint32_t shadowMapIdx = 0;
};

struct PointLight {
  glm::vec3 position;
  float constant;
  glm::vec3 ambient;
  float linear;
  glm::vec3 diffuse;
  glm::vec3 specular;
  glm::mat4 lightSpaceMatrix;
  uint32_t shadowMapIdx = 0;
};

struct DirectionalLight {
  glm::vec3 direction;
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
  glm::mat4 lightSpaceMatrix;
  uint32_t shadowMapIdx = 0;
};

struct Skylight {
  float intesnity;
  uint32_t cubemapIdx;
};

struct Asset {
  std::vector<uint32_t> meshes;
  std::filesystem::path path;
};

struct Entity {
  glm::mat4 matrix = glm::mat4{1.0f};
  uint32_t assetIdx = 0;
  Buffer uniform;
};

struct EngineConfig {
  Transform transform;
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

struct AssimpSampler {
  const aiTextureMapMode u;
  const aiTextureMapMode v;
  const GLTFMagFilter mag;
  const GLTFMinFilter min;

  vk::SamplerCreateInfo toVkSampler(const Image& image,
                                    float maxSamplerAnisotropy) const;
};

struct TextureCreateInfo {
  std::filesystem::path path;
  AssimpSampler sampler;
  uint32_t textureIdx;
};

struct ImageData {
  uint32_t width, height;
  uint8_t* data;
};

class Engine {
 public:
  Transform transform;

  bool shadowsGenerated = false;

  std::vector<Entity> entities;
  std::vector<Material> materials;
  std::vector<Texture> textures;
  std::vector<Asset> assets;
  std::vector<Mesh> meshes;

  vk::Sampler shadowMapSampler;

  DirectionalLight directionalLight;
  std::vector<PointLight> pointLights;
  std::vector<SpotLight> spotLights;

  Buffer directionalLightUniform;
  Buffer pointLightsBuffer;
  Buffer spotLightsBuffer;

  Camera camera;
  Texture skybox;

  bool isRunning = true;

  Engine(const Display& display, const GPU& gpu, const EngineConfig& config);

  void drawFrame(uint64_t deltaTime);
  void drawSkybox(const SkyboxPassFrameData& data);
  void processInput(uint64_t deltaTime);
  void destroyTexture(const Texture& texture);
  void destroy();

 private:
  Display display;
  GPU gpu;

  bool shouldBeResized = false;

  void loadStatic();
  void loadConfig(const EngineConfig& state);

  void loadAsset(const std::filesystem::path& path);
  void processNode(Asset& asset,
                   const aiScene* scene,
                   const aiNode* node,
                   std::vector<TextureCreateInfo>& tasks);
  void loadMesh(Asset& asset,
                const aiScene* scene,
                const aiMesh* mesh,
                std::vector<TextureCreateInfo>& tasks);
  void loadMaterial(Asset& asset,
                    Mesh& mesh,
                    const aiMaterial* assimpMaterial,
                    std::vector<TextureCreateInfo>& tasks);
  std::pair<Texture, AssimpSampler> loadTexture(
      const aiMaterial* material,
      const aiTextureType textureType);
  Image loadImage(const std::filesystem::path& path,
                  vk::Format format = vk::Format::eR8G8B8A8Srgb);
  Image loadCubemap(const std::string& type);

  Texture createShadowMap();

  void createDescriptors();
  void prepareUniformsAndDescriptors();
  void loadSkybox();
  void drawShadows(const ShadowPassFrameData& frameData);
  void drawEntities(const MainPassFrameData& frameData);
  void drawEntity(const uint32_t entityIdx, const uint32_t meshIdx);
  void drawEntityShadow(const uint32_t entityIdx, const uint32_t meshIdx);
  void drawMesh(const uint32_t meshIdx);
};
