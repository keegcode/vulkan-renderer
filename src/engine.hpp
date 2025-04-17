#pragma once

#include <assimp/scene.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <filesystem>

#include "display.hpp"
#include "gpu.hpp"

const std::array<std::string, 6> CUBEMAP_FILES{
  "left",
  "right",
  "top",
  "bottom",
  "front",
  "back"
};

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

struct Material {
  alignas(16) glm::vec3 specular = glm::vec3{1.0};
  alignas(16) glm::vec3 emissive = glm::vec3{0.0};
  alignas(16) glm::vec3 color = glm::vec3{1.0};
  float shininess = 32.0;
  Buffer uniform;
  uint32_t diffuseTextureIdx = 0;
  uint32_t specularTextureIdx = 0;
};

enum class TextureType {
  BaseColor,
  Specular,
};

struct Texture {
  Image image;
  TextureType type;
  std::string path;
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

class Engine {
 public:
  Pipeline pipeline;
  Pipeline skyboxPipeline;
  Image skybox;

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

  bool isRunning = true;

  Engine(const Display& display, const GPU& gpu);

  void init(const EngineConfig& state);

  void drawFrame(float deltaTime);
  void drawSkybox(const FrameData& data);
  void processInput(float deltaTime);
  void destroy();

 private:
  Display display;
  GPU gpu;

  bool shouldBeResized = false;

  void createPipeline();
  void loadStatic();
  void loadConfig(const EngineConfig& state);

  void loadAsset(const std::filesystem::path& path);
  void processNode(Asset& asset, const aiScene* scene, const aiNode* node);
  void loadMesh(Asset& asset, const aiScene* scene, const aiMesh* mesh);
  void loadMaterial(Asset& asset, Mesh& mesh, const aiMaterial* assimpMaterial);

  Image loadImage(const std::filesystem::path& path);
  void loadSkybox();
};
