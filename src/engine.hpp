#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <assimp/material.h>
#include <assimp/scene.h>
#include <cstdint>
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/matrix.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "display.hpp"
#include "gpu.hpp"

const std::array<std::string, 6> CUBEMAP_FILES{"right",  "left",  "top",
                                               "bottom", "front", "back"};

const glm::mat4 shadowProjection =
    glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, 800.0f);

const std::vector<std::pair<glm::vec3, glm::vec3>> shadowCubeSides{
    {
        glm::vec3{1.0, 0.0, 0.0},
        glm::vec3{0.0, 1.0f, 0.0},
    },
    {
        glm::vec3{-1.0, 0.0, 0.0},
        glm::vec3{0.0, 1.0f, 0.0},
    },
    {
        glm::vec3{0.0, 1.0, 0.0},
        glm::vec3{0.0, 0.0f, 1.0f},
    },
    {
        glm::vec3{0.0, -1.0, 0.0},
        glm::vec3{0.0, 0.0f, 1.0},
    },
    {
        glm::vec3{0.0, 0.0, 1.0f},
        glm::vec3{0.0, 1.0f, 0.0},
    },
    {
        glm::vec3{0.0, 0.0, -1.0f},
        glm::vec3{0.0, 1.0f, 0.0},
    },
};

struct Transform {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

struct ShadowCubeTransform {
  glm::mat4 model;
  glm::mat4 projection[6];
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
  glm::vec3 color = glm::vec3{1.0f};
  glm::vec3 emissive = glm::vec3{0.0f};
  float alphaCutoff = 0.001f;
  float transmissionFactor = 0.0f;
  float roughness = 1.0f;
  float metallic = 0.0f;
  uint32_t normalTextureIdx = 1;
  uint32_t diffuseTextureIdx = 0;
  uint32_t metallicRoughnessTextureIdx = 0;
  uint32_t emissiveTextureIdx = 0;
  vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eBack;
  AlphaMode alphaMode = AlphaMode::Opaque;
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
  uint32_t shadowMapIdx;
  glm::mat4 lightSpaceMatrix;
  bool shadowFactor = 1.0f;
};

struct PointLight {
  glm::vec3 position;
  float constant;
  glm::vec3 ambient;
  float linear;
  glm::vec3 diffuse;
  glm::vec3 specular;
  uint32_t shadowMapIdx;
  float shadowFactor = 1.0f;
  float farPlane = 300.0f;
};

struct DirectionalLight {
  glm::vec3 direction;
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
  uint32_t shadowMapIdx;
  glm::mat4 lightSpaceMatrix;
  float shadowFactor;
};

struct Skylight {
  float intesnity = 1.0f;
  uint32_t cubemapIdx = 0;
};

struct Model {
  std::vector<uint32_t> meshes;
  std::filesystem::path path;
};

struct Entity {
  glm::mat4 matrix = glm::mat4{1.0f};
  uint32_t modelIdx = 0;
};

struct EngineConfig {
  Skylight skylight;
  Transform transform;
  DirectionalLight directionalLight;
  std::vector<PointLight> pointLights;
  std::vector<SpotLight> spotLights;
  std::vector<Entity> entities;
  std::vector<std::filesystem::path> models;
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
  std::vector<Texture> shadowMaps;
  std::vector<Texture> shadowCubes;
  std::vector<Model> models;
  std::vector<Mesh> meshes;

  vk::Sampler shadowMapSampler;

  DirectionalLight directionalLight;
  Skylight skylight;
  std::vector<PointLight> pointLights;
  std::vector<SpotLight> spotLights;

  Buffer transformUniform;
  Buffer shadowCubeTransformUniform;
  Buffer directionalLightUniform;
  Buffer skylightUniform;
  Buffer pointLightsBuffer;
  Buffer spotLightsBuffer;
  Buffer materialsBuffer;
  Buffer entitiesBuffer;

  Camera camera;
  Texture skybox;

  bool isRunning = true;

  Engine(const Display& display, const GPU& gpu, const EngineConfig& config);

  void drawFrame(uint64_t deltaTime);
  void drawSkybox(const SkyboxPassPushConstant& data);
  void processInput(uint64_t deltaTime);
  void destroyTexture(const Texture& texture);
  void destroy();

 private:
  Display display;
  GPU gpu;

  bool shouldBeResized = false;

  void loadStatic();
  void loadConfig(const EngineConfig& state);

  void loadModel(const std::filesystem::path& path);
  void processNode(Model& model,
                   const aiScene* scene,
                   const aiNode* node,
                   std::vector<TextureCreateInfo>& tasks);
  void loadMesh(Model& model,
                const aiScene* scene,
                const aiMesh* mesh,
                std::vector<TextureCreateInfo>& tasks);
  void loadMaterial(Model& model,
                    Mesh& mesh,
                    const aiMaterial* assimpMaterial,
                    std::vector<TextureCreateInfo>& tasks);
  std::pair<Texture, AssimpSampler> loadTexture(
      const aiMaterial* material,
      const aiTextureType textureType);
  Image loadImage(const std::filesystem::path& path,
                  vk::Format format = vk::Format::eR8G8B8A8Srgb);
  Image loadCubemap(const std::string& type);

  void prepareDescriptors();
  void loadSkybox();
  void drawShadows(const ShadowPassPushConstant& pushConstant);
  void drawShadowCubes(const ShadowCubePassPushConstant& pushConstant);
  void drawEntities(const MainPassPushConstant& pushConstant);
  void drawEntity(const uint32_t meshIdx);
  void drawEntityShadow(const uint32_t meshIdx);
  void drawMesh(const uint32_t meshIdx);
};
