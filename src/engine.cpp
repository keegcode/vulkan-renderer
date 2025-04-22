#include "engine.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <assimp/Importer.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <glm/matrix.hpp>
#include <vector>

#include <glm/common.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "gpu.hpp"
#include "stb_image.h"

Engine::Engine(const Display& d, const GPU& g) : display{d}, gpu{g} {};

void Engine::init(const EngineConfig& config) {
  gpu.createInstance();
  gpu.pickPhysicalDevice();
  gpu.pickDevice();
  gpu.createSwapchain();
  gpu.createAllocator();
  gpu.createQueue();
  gpu.createSyncPrimitives();
  gpu.createCommandPool();
  gpu.createCommandBuffer();
  gpu.createSampler();
  gpu.createDescriptorSetLayouts();

  loadSkybox();
  loadReflectionCube();
  loadStatic();
  loadConfig(config);

  gpu.createImages();
  gpu.createViewportAndScissors();
  createPipeline();
}

void Engine::drawFrame(float deltaTime) {
  gpu.waitForFence();

  if (shouldBeResized) {
    gpu.rebuiltSwapchain();
    shouldBeResized = false;
  }

  int32_t imageIndex = gpu.acquireNextImage();

  if (imageIndex == -1) {
    gpu.rebuiltSwapchain();
    return;
  }

  gpu.resetFence();

  gpu.beginRendering(static_cast<uint32_t>(imageIndex));

  projection.view =
      glm::lookAt(camera.position, camera.position + camera.front, camera.up);

  FrameData frameData{};
  frameData.model = projection.model;
  frameData.view = projection.view;
  frameData.perspective = projection.perspective;
  frameData.spotLights = spotLights.size();
  frameData.pointLights = pointLights.size();
  frameData.cameraPos = camera.position;

  drawEntities(frameData);
  drawSkybox(frameData);
  
  gpu.endRendering();
  gpu.submit(static_cast<uint32_t>(imageIndex));
};

void Engine::drawSkybox(const FrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 skyboxPipeline.pipeline);

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);

  gpu.commandBuffer.pushConstants(
      skyboxPipeline.layout,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
      sizeof(FrameData), &frameData);

  std::vector<vk::DescriptorBufferBindingInfoEXT> sceneBidningInfo{
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT |
                    vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(skyboxDescriptor.address.deviceAddress)};

  std::array<vk::DeviceSize, 1> offsets{0};
  std::vector<vk::DeviceSize> descriptorOffsets{0};
  std::vector<uint32_t> descriptorIndices{0};

  Mesh& mesh = meshes[0];

  gpu.commandBuffer.bindDescriptorBuffersEXT(sceneBidningInfo, gpu.dld);

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer,
                                      offsets.data());

  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);

  gpu.commandBuffer.setDescriptorBufferOffsetsEXT(
      vk::PipelineBindPoint::eGraphics, pipeline.layout, 0, 1,
      descriptorIndices.data(), descriptorOffsets.data(), gpu.dld);

  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}

void Engine::processInput(float deltaTime) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_ESCAPE) {
      isRunning = false;
      break;
    }
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      isRunning = false;
      break;
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
      shouldBeResized = true;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_RIGHT) {
      SDL_SetWindowRelativeMouseMode(display.window, true);
      SDL_SetWindowMouseGrab(display.window, true);
      camera.mode = CameraMode::Move;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event.button.button == SDL_BUTTON_RIGHT) {
      SDL_SetWindowRelativeMouseMode(display.window, false);
      SDL_SetWindowMouseGrab(display.window, false);
      camera.mode = CameraMode::Fixed;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && camera.mode == CameraMode::Move) {
      switch (event.key.scancode) {
        case SDL_SCANCODE_W:
          camera.position += camera.front * camera.velocity * deltaTime;
          break;
        case SDL_SCANCODE_S:
          camera.position -= camera.front * camera.velocity * deltaTime;
          break;
        case SDL_SCANCODE_A:
          camera.position -= camera.right * camera.velocity * deltaTime;
          break;
        case SDL_SCANCODE_D:
          camera.position += camera.right * camera.velocity * deltaTime;
          break;
        default:
          break;
      }
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION &&
        camera.mode == CameraMode::Move) {
      camera.yaw += event.motion.xrel * camera.sensitivity;
      camera.pitch += -event.motion.yrel * camera.sensitivity;
      camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);

      glm::vec3 front{0.0f};

      front.x = glm::cos(glm::radians(camera.yaw)) *
                glm::cos(glm::radians(camera.pitch));
      front.y = glm::sin(glm::radians(camera.pitch));
      front.z = glm::sin(glm::radians(camera.yaw)) *
                glm::cos(glm::radians(camera.pitch));

      camera.front = glm::normalize(front);
      camera.right = glm::normalize(glm::cross(camera.front, camera.up));
    }
  }
};

void Engine::destroy() {
  gpu.device.waitIdle();

  gpu.destroyDescriptor(lightsDescriptor);
  gpu.destroyDescriptor(materialsDescriptor);
  gpu.destroyDescriptor(entitiesDescriptor);
  gpu.destroyDescriptor(texturesDescriptor);
  gpu.destroyDescriptor(skyboxDescriptor);
  gpu.destroyDescriptor(reflectionCubeDescriptor);

  for (Texture& texture : textures) {
    gpu.destroyImage(texture.image);
  }

  gpu.destroyImage(skybox);
  gpu.destroyImage(reflectionCube);

  for (Mesh& mesh : meshes) {
    gpu.destroyBuffer(mesh.vertexBuffer);
    gpu.destroyBuffer(mesh.indexBuffer);
  }

  for (PointLight& l : pointLights) {
    gpu.destroyBuffer(l.uniform);
  }

  for (SpotLight& l : spotLights) {
    gpu.destroyBuffer(l.uniform);
  }

  for (Material& m : materials) {
    gpu.destroyBuffer(m.uniform);
  }

  for (Entity& e : entities) {
    gpu.destroyBuffer(e.uniform);
  }

  gpu.destroyBuffer(directionalLight.uniform);

  gpu.destroyPipeline(pipeline);
  gpu.destroyPipeline(skyboxPipeline);

  gpu.destroySwapchainResources();

  gpu.destroy();
  display.destroy();
};

void Engine::createPipeline() {
  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{
      gpu.textureLayout, gpu.uniformLayout, gpu.lightLayout,
      gpu.uniformLayout, gpu.skyboxLayout,
  };

  pipeline = gpu.createEntityPipeline(
      gpu.loadShader("./shaders/shader.vert.glsl.spv",
                     vk::ShaderStageFlagBits::eVertex),
      gpu.loadShader("./shaders/shader.frag.glsl.spv",
                     vk::ShaderStageFlagBits::eFragment),
      descriptorSetLayouts);

  descriptorSetLayouts = {
      gpu.skyboxLayout,
  };

  skyboxPipeline = gpu.createSkyboxPipeline(
      gpu.loadShader("./shaders/cubemap.vert.glsl.spv",
                     vk::ShaderStageFlagBits::eVertex),
      gpu.loadShader("./shaders/cubemap.frag.glsl.spv",
                     vk::ShaderStageFlagBits::eFragment),
      descriptorSetLayouts);
};

Image Engine::loadImage(const std::filesystem::path& path) {
  int height, width;
  uint8_t* data = stbi_load(path.c_str(), &width, &height, 0, STBI_rgb_alpha);

  assert(data != nullptr);

  Image texture = gpu.createTexture2D(
      data, vk::Extent2D{}.setWidth(width).setHeight(height));

  stbi_image_free(data);

  return texture;
}

void Engine::loadConfig(const EngineConfig& config) {
  std::vector<glm::vec3> lightPositions{};

  projection = config.projection;

  lightsDescriptor = gpu.createUniformDescriptor(
      config.spotLights.size() + pointLights.size() + 1, gpu.lightLayout);

  directionalLight = config.directionalLight;
  lightPositions.push_back(config.directionalLight.position);

  directionalLight.uniform =
      gpu.createBuffer(&directionalLight, offsetof(DirectionalLight, uniform),
                       vk::BufferUsageFlagBits::eUniformBuffer |
                           vk::BufferUsageFlagBits::eShaderDeviceAddress);

  gpu.setDescriptorUniformBuffer(lightsDescriptor, directionalLight.uniform, 0,
                                 0);

  for (size_t i = 0; i < config.pointLights.size(); i++) {
    PointLight light = config.pointLights[i];
    lightPositions.push_back(light.position);

    light.uniform =
        gpu.createBuffer(&light, offsetof(PointLight, uniform),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);

    gpu.setDescriptorUniformBuffer(lightsDescriptor, light.uniform, i, 1);

    pointLights.push_back(light);
  }

  for (size_t i = 0; i < config.spotLights.size(); i++) {
    SpotLight light = config.spotLights[i];
    lightPositions.push_back(light.position);

    light.uniform =
        gpu.createBuffer(&light, offsetof(SpotLight, uniform),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);

    gpu.setDescriptorUniformBuffer(lightsDescriptor, light.uniform, i, 2);

    spotLights.push_back(light);
  }

  for (const std::filesystem::path& path : config.assets) {
    loadAsset(path);
  }

  for (size_t i = 0; i < lightPositions.size(); i++) {
    Entity entity{};
    entity.matrix = glm::scale(
        glm::translate(glm::mat4{1.0f}, lightPositions[i]), glm::vec3{1.0});
    entity.assetIdx = 0;
    entities.push_back(entity);
  }

  for (size_t i = 0; i < config.entities.size(); i++) {
    entities.push_back(config.entities[i]);
  }

  entitiesDescriptor =
      gpu.createUniformDescriptor(entities.size(), gpu.uniformLayout);

  for (size_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    entity.uniform =
        gpu.createBuffer(&entity, sizeof(glm::mat4),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);
    gpu.setDescriptorUniformBuffer(entitiesDescriptor, entity.uniform, i, 0);
  }

  materialsDescriptor =
      gpu.createUniformDescriptor(materials.size(), gpu.uniformLayout);

  texturesDescriptor =
      gpu.createTextureDescriptor(materials.size(), gpu.textureLayout);

  for (size_t i = 0; i < materials.size(); i++) {
    Material& material = materials[i];

    material.uniform =
        gpu.createBuffer(&material, offsetof(Material, uniform),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);

    gpu.setDescriptorUniformBuffer(materialsDescriptor, material.uniform, i, 0);

    Texture& diffuseMap = textures[material.diffuseTextureIdx];
    Texture& specularMap = textures[material.specularTextureIdx];

    gpu.setDescriptorImage(texturesDescriptor, diffuseMap.image,
                           gpu.diffuseSampler, i, 0);

    gpu.setDescriptorImage(texturesDescriptor, specularMap.image,
                           gpu.specularSampler, i, 1);
  }
}

void Engine::loadStatic() {
  Texture texture{};
  texture.image = loadImage("./textures/default.png");
  texture.path = "./textures/default.png";
  texture.type = TextureType::BaseColor;

  textures.push_back(texture);

  Material defaultMaterial{};
  defaultMaterial.emissive = glm::vec3{0.0f};
  defaultMaterial.specular = glm::vec3{1.0f};
  defaultMaterial.shininess = 32.0;
  defaultMaterial.color = glm::vec3{0.5f};

  Material lightMaterial{};
  lightMaterial.emissive = glm::vec3{1.0f};
  lightMaterial.color = glm::vec3{1.0f};
  lightMaterial.specular = glm::vec3{1.0f};
  lightMaterial.shininess = 32.0;

  materials.push_back(defaultMaterial);
  materials.push_back(lightMaterial);

  loadAsset("./assets/Cube/glTF/Cube.gltf");
};

void Engine::loadAsset(const std::filesystem::path& path) {
  Assimp::Importer importer{};

  const aiScene* scene = importer.ReadFile(
      path.c_str(),
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes);

  assert(scene != nullptr);

  Asset asset{};
  asset.path = path;
  

  processNode(asset, scene, scene->mRootNode);

  importer.FreeScene();
  assets.push_back(asset);
}

void Engine::loadMesh(Asset& asset,
                      const aiScene* scene,
                      const aiMesh* assimpMesh) {
  Mesh mesh{};

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  for (size_t j = 0; j < assimpMesh->mNumVertices; j++) {
    Vertex vertex{};

    vertex.position[0] = assimpMesh->mVertices[j].x;
    vertex.position[1] = assimpMesh->mVertices[j].y;
    vertex.position[2] = assimpMesh->mVertices[j].z;

    vertex.normals[0] = assimpMesh->mNormals[j].x;
    vertex.normals[1] = assimpMesh->mNormals[j].y;
    vertex.normals[2] = assimpMesh->mNormals[j].z;

    vertex.uv[0] = assimpMesh->mTextureCoords[0][j].x;
    vertex.uv[1] = assimpMesh->mTextureCoords[0][j].y;

    if (assimpMesh->HasVertexColors(0)) {
      vertex.clr[0] = assimpMesh->mColors[0][j].r;
      vertex.clr[1] = assimpMesh->mColors[0][j].g;
      vertex.clr[2] = assimpMesh->mColors[0][j].b;
      vertex.clr[3] = assimpMesh->mColors[0][j].a;
    } else {
      vertex.clr[0] = 1.0f;
      vertex.clr[1] = 1.0f;
      vertex.clr[2] = 1.0f;
      vertex.clr[3] = 1.0f;
    }

    vertices.push_back(vertex);
  }

  for (size_t j = 0; j < assimpMesh->mNumFaces; j++) {
    assert(assimpMesh->mFaces[j].mNumIndices == 3);
    indices.push_back(assimpMesh->mFaces[j].mIndices[0]);
    indices.push_back(assimpMesh->mFaces[j].mIndices[1]);
    indices.push_back(assimpMesh->mFaces[j].mIndices[2]);
  }

  mesh.vertexBuffer =
      gpu.createBuffer(vertices.data(), sizeof(Vertex) * vertices.size(),
                       vk::BufferUsageFlagBits::eVertexBuffer);

  mesh.indexBuffer =
      gpu.createBuffer(indices.data(), sizeof(uint32_t) * indices.size(),
                       vk::BufferUsageFlagBits::eIndexBuffer);

  mesh.indicesCount = indices.size();

  loadMaterial(asset, mesh, scene->mMaterials[assimpMesh->mMaterialIndex]);

  asset.meshes.push_back(meshes.size());
  meshes.push_back(mesh);
}

void Engine::loadMaterial(Asset& asset,
                          Mesh& mesh,
                          const aiMaterial* assimpMaterial) {
  Material material{};

  aiColor3D emissiveColor{0.0, 0.0, 0.0};
  if (assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) ==
      aiReturn_SUCCESS) {
    material.emissive =
        glm::vec3{emissiveColor.r, emissiveColor.g, emissiveColor.b};
  };

  aiColor3D specularColor{1.0, 1.0, 1.0};
  if (assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) ==
      aiReturn_SUCCESS) {
    material.specular =
        glm::vec3{specularColor.r, specularColor.g, specularColor.b};
  };

  aiColor3D diffuseColor{1.0, 1.0, 1.0};
  if (assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) ==
      aiReturn_SUCCESS) {
    material.color = glm::vec3{diffuseColor.r, diffuseColor.g, diffuseColor.b};
  };

  float shininess = 32.0;
  if (assimpMaterial->Get(AI_MATKEY_SHININESS, shininess) == aiReturn_SUCCESS) {
    material.shininess = std::clamp(shininess, 4.0f, 32.0f);
  };

  int twoSided = 0;
  if (assimpMaterial->Get(AI_MATKEY_TWOSIDED, twoSided) == aiReturn_SUCCESS) {
    material.cullMode = vk::CullModeFlagBits::eNone;
  };

  aiString alphaModeStr;
  if (assimpMaterial->Get("$mat.gltf.alphaMode", 0, 0, alphaModeStr) ==
      aiReturn_SUCCESS) {
    std::string mode = alphaModeStr.C_Str();
    if (mode == "MASK") {
      material.alphaMode = AlphaMode::Mask;
      material.alphaCutoff = 0.5f;
    }
  }

  float transmissionFactor = 0.0f;
  if (assimpMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmissionFactor) ==
      aiReturn_SUCCESS) {
    material.transmissionFactor = transmissionFactor;
  };

  // float roughness = 1.0f;
  // if (assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) ==
  //     aiReturn_SUCCESS) {
  //   material.rougness = roughness;
  // };

  if (asset.path == "./assets/DamagedHelmet/glTF/DamagedHelmet.gltf") {
    material.rougness = 0.8f;
  }

  if (asset.path == "./assets/Sponza/glTF/Sponza.gltf") {
    material.rougness = 0.95f;
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
    aiString diffuseMapPath;
    assimpMaterial->GetTexture(aiTextureType::aiTextureType_BASE_COLOR, 0,
                               &diffuseMapPath);
    Texture diffuseMap{};
    diffuseMap.type = TextureType::BaseColor;
    diffuseMap.image =
        loadImage(asset.path.parent_path().append(diffuseMapPath.C_Str()));
    diffuseMap.path = asset.path.parent_path().append(diffuseMapPath.C_Str());
    material.diffuseTextureIdx = textures.size();
    textures.push_back(diffuseMap);
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
    aiString specularMapPath;
    assimpMaterial->GetTexture(aiTextureType::aiTextureType_SPECULAR, 0,
                               &specularMapPath);
    Texture specularMap{};
    specularMap.type = TextureType::Specular;
    specularMap.image =
        loadImage(asset.path.parent_path().append(specularMapPath.C_Str()));
    specularMap.path = asset.path.parent_path().append(specularMapPath.C_Str());
    material.specularTextureIdx = textures.size();
    textures.push_back(specularMap);
  }

  mesh.materialIdx = materials.size();
  materials.push_back(material);
}

void Engine::processNode(Asset& asset,
                         const aiScene* scene,
                         const aiNode* node) {
  for (size_t i = 0; i < node->mNumMeshes; i++) {
    loadMesh(asset, scene, scene->mMeshes[node->mMeshes[i]]);
  }

  for (size_t i = 0; i < node->mNumChildren; i++) {
    processNode(asset, scene, node->mChildren[i]);
  }
};

void Engine::loadReflectionCube() {
  int height, width;
  std::array<uint8_t*, 6> images{};

  for (size_t i = 0; i < 6; i++) {
    std::string file = CUBEMAP_FILES[i];
    uint8_t* data =
        stbi_load(std::string{"./textures/reflection/" + file + ".png"}.c_str(),
                  &width, &height, nullptr, STBI_rgb_alpha);
    assert(data != nullptr);
    images[i] = data;
  }

  Image cubemap = gpu.createCubemapTexture(
      images, vk::Extent2D{}.setWidth(width).setHeight(height));

  for (const auto& data : images) {
    stbi_image_free(data);
  }

  reflectionCube = cubemap;
  reflectionCubeDescriptor = gpu.createTextureDescriptor(1, gpu.skyboxLayout);
  gpu.setDescriptorImage(reflectionCubeDescriptor, reflectionCube,
                         gpu.skyboxSampler, 0, 0);
}

void Engine::loadSkybox() {
  int height, width;
  std::array<uint8_t*, 6> images{};

  for (size_t i = 0; i < 6; i++) {
    std::string file = CUBEMAP_FILES[i];
    uint8_t* data =
        stbi_load(std::string{"./textures/skybox/" + file + ".png"}.c_str(),
                  &width, &height, nullptr, STBI_rgb_alpha);
    assert(data != nullptr);
    images[i] = data;
  }

  Image cubemap = gpu.createCubemapTexture(
      images, vk::Extent2D{}.setWidth(width).setHeight(height));

  for (const auto& data : images) {
    stbi_image_free(data);
  }

  skybox = cubemap;
  skyboxDescriptor = gpu.createTextureDescriptor(1, gpu.skyboxLayout);
  gpu.setDescriptorImage(skyboxDescriptor, skybox, gpu.skyboxSampler, 0, 0);
}

void Engine::drawEntities(const FrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 pipeline.pipeline);

  std::vector<vk::DeviceSize> offsets = {0};

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);

  gpu.commandBuffer.pushConstants(
      pipeline.layout,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
      sizeof(FrameData), &frameData);

  std::vector<vk::DescriptorBufferBindingInfoEXT> sceneBidningInfo{
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT |
                    vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(texturesDescriptor.address.deviceAddress),
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(materialsDescriptor.address.deviceAddress),
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(lightsDescriptor.address.deviceAddress),
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(entitiesDescriptor.address.deviceAddress),
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                    vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT)
          .setAddress(reflectionCubeDescriptor.address.deviceAddress)};

  gpu.commandBuffer.bindDescriptorBuffersEXT(sceneBidningInfo, gpu.dld);

  vk::Bool32 enables[1] = {false};
  gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);

  for (size_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    Asset& asset = assets[entity.assetIdx];
    for (const uint32_t meshIdx : asset.meshes) {
      Mesh& mesh = meshes[meshIdx];
      Material& material = materials[mesh.materialIdx];

      if (material.alphaMode != AlphaMode::Opaque) {
        vk::Bool32 enables[1] = {true};
        gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);
      } else {
        vk::Bool32 enables[1] = {false};
        gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);
      }

      gpu.commandBuffer.setCullMode(material.cullMode);

      gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer,
                                          offsets.data());

      gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                        vk::IndexType::eUint32);

      std::vector<uint32_t> descriptorIndices{0, 1, 2, 3, 4};

      std::vector<vk::DeviceSize> descriptorOffsets{
          texturesDescriptor.size * mesh.materialIdx,
          materialsDescriptor.size * mesh.materialIdx, 0,
          entitiesDescriptor.size * i, 0};

      gpu.commandBuffer.setDescriptorBufferOffsetsEXT(
          vk::PipelineBindPoint::eGraphics, pipeline.layout, 0, 5,
          descriptorIndices.data(), descriptorOffsets.data(), gpu.dld);

      gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
    }
  }
}
