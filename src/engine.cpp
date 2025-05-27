#include "engine.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <vulkan/vulkan_core.h>
#include <unordered_map>

#include <algorithm>
#include <assimp/Importer.hpp>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/matrix.hpp>
#include <thread>
#include <vector>

#include <glm/common.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "gpu.hpp"
#include "stb_image.h"

Engine::Engine(const Display& d, const GPU& g, const EngineConfig& config)
    : display{d}, gpu{g} {
  loadSkybox();
  loadStatic();
  loadConfig(config);
  prepareDescriptors();
};

void Engine::drawFrame(uint64_t deltaTime) {
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
  gpu.commandBuffer.reset();

  gpu.beginRecordingCommands();

  transform.model = transform.model;
  transform.view =
      glm::lookAt(camera.position, camera.position + camera.front, camera.up);
  transform.projection = transform.projection;

  vmaCopyMemoryToAllocation(gpu.allocator, &transform,
                            transformUniform.allocation, 0, sizeof(Transform));
  if (!shadowsGenerated) {
    ShadowPassFrameData shadowPassFrameData{};
    shadowPassFrameData.model = transform.model;
    shadowPassFrameData.lightSpaceMatrix = directionalLight.lightSpaceMatrix;

    if (directionalLight.shadows) {
      gpu.beginShadowPass(shadowMaps[directionalLight.shadowMapIdx]);
      drawShadows(shadowPassFrameData);
      gpu.commandBuffer.endRendering();
    }

    for (const PointLight& pointLight : pointLights) {
      if (!pointLight.shadows) {
        continue;
      }

      gpu.beginShadowPass(shadowMaps[pointLight.shadowMapIdx]);
      shadowPassFrameData.model = transform.model;
      shadowPassFrameData.lightSpaceMatrix = pointLight.lightSpaceMatrix;
      drawShadows(shadowPassFrameData);
      gpu.commandBuffer.endRendering();
    }

    for (const SpotLight& spotLight : spotLights) {
      if (!spotLight.shadows) {
        continue;
      }

      gpu.beginShadowPass(shadowMaps[spotLight.shadowMapIdx]);
      shadowPassFrameData.model = transform.model;
      shadowPassFrameData.lightSpaceMatrix = spotLight.lightSpaceMatrix;
      drawShadows(shadowPassFrameData);
      gpu.commandBuffer.endRendering();
    }

    shadowsGenerated = true;
  }

  gpu.beginMainPass(static_cast<uint32_t>(imageIndex));

  MainPassFrameData mainPassFrameData{};
  mainPassFrameData.spotLights = static_cast<uint32_t>(spotLights.size());
  mainPassFrameData.pointLights = static_cast<uint32_t>(pointLights.size());
  mainPassFrameData.cameraPos = camera.position;

  drawEntities(mainPassFrameData);

  SkyboxPassFrameData skyboxFrameData{};
  skyboxFrameData.matrix = transform.projection *
                           glm::mat4{glm::mat3{transform.view}} *
                           transform.model;

  drawSkybox(skyboxFrameData);

  gpu.commandBuffer.endRendering();
  gpu.commandBuffer.end();

  gpu.submit(static_cast<uint32_t>(imageIndex));
};

void Engine::drawSkybox(const SkyboxPassFrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 gpu.skyboxPipeline.pipeline);

  gpu.commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, gpu.skyboxPipeline.layout, 0,
      gpu.skyboxPipeline.descriptorSets.size(),
      gpu.skyboxPipeline.descriptorSets.data(), 0, nullptr);

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);

  gpu.commandBuffer.setDepthWriteEnable(0);

  gpu.commandBuffer.pushConstants(
      gpu.skyboxPipeline.layout,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
      sizeof(SkyboxPassFrameData), &frameData);

  std::array<vk::DeviceSize, 1> offsets{0};

  Mesh& mesh = meshes[0];

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer,
                                      offsets.data());

  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);

  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}

void Engine::processInput(uint64_t deltaTime) {
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
          camera.position +=
              camera.front * camera.velocity * (1.0f / deltaTime);
          break;
        case SDL_SCANCODE_S:
          camera.position -=
              camera.front * camera.velocity * (1.0f / deltaTime);
          break;
        case SDL_SCANCODE_A:
          camera.position -=
              camera.right * camera.velocity * (1.0f / deltaTime);
          break;
        case SDL_SCANCODE_D:
          camera.position +=
              camera.right * camera.velocity * (1.0f / deltaTime);
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

void Engine::destroyTexture(const Texture& texture) {
  gpu.destroyImage(texture.image);
  gpu.destroySampler(texture.sampler);
};

void Engine::destroy() {
  gpu.device.waitIdle();

  for (Texture& texture : textures) {
    destroyTexture(texture);
  }

  for (Texture& texture : shadowMaps) {
    gpu.destroyImage(texture.image);
  }

  gpu.destroySampler(shadowMapSampler);

  destroyTexture(skybox);

  for (Mesh& mesh : meshes) {
    gpu.destroyBuffer(mesh.vertexBuffer);
    gpu.destroyBuffer(mesh.indexBuffer);
  }

  gpu.destroyBuffer(transformUniform);
  gpu.destroyBuffer(directionalLightUniform);
  gpu.destroyBuffer(skylightUniform);
  gpu.destroyBuffer(spotLightsBuffer);
  gpu.destroyBuffer(pointLightsBuffer);
  gpu.destroyBuffer(entitiesBuffer);
  gpu.destroyBuffer(materialsBuffer);

  gpu.destroySwapchainResources();

  gpu.destroy();
  display.destroy();
};

Image Engine::loadImage(const std::filesystem::path& path, vk::Format format) {
  int height{}, width{};
  uint8_t* data =
      stbi_load(path.string().c_str(), &width, &height, 0, STBI_rgb_alpha);

  assert(data != nullptr);

  Image texture =
      gpu.createTexture2D(data,
                          vk::Extent2D{}
                              .setWidth(static_cast<uint32_t>(width))
                              .setHeight(static_cast<uint32_t>(height)),
                          format);

  stbi_image_free(data);

  return texture;
}

void Engine::loadConfig(const EngineConfig& config) {
  transform = config.transform;
  directionalLight = config.directionalLight;
  skylight = config.skylight;

  pointLights = config.pointLights;
  spotLights = config.spotLights;

  entities = config.entities;

  for (const std::filesystem::path& path : config.assets) {
    loadAsset(path);
  }
}

void Engine::loadStatic() {
  vk::SamplerCreateInfo samplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setCompareEnable(0)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(
              std::min(16.0f, gpu.physicalDeviceProperties.properties.limits
                                  .maxSamplerAnisotropy));

  vk::SamplerCreateInfo shadowMapSamplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
          .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
          .setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
          .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
          .setCompareEnable(1)
          .setCompareOp(vk::CompareOp::eLess)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(
              std::min(16.0f, gpu.physicalDeviceProperties.properties.limits
                                  .maxSamplerAnisotropy));

  shadowMapSampler = gpu.createSampler(shadowMapSamplerCreateInfo);

  Texture texture{};
  texture.image = loadImage("./textures/default.png");
  texture.type = TextureType::BaseColor;
  texture.sampler = gpu.createSampler(samplerCreateInfo);

  Texture normal{};
  normal.image =
      loadImage("./textures/default_normal.png", vk::Format::eR8G8B8A8Unorm);
  normal.type = TextureType::Normal;
  normal.sampler = gpu.createSampler(samplerCreateInfo);

  textures.push_back(texture);
  textures.push_back(normal);

  Material defaultMaterial{};
  defaultMaterial.emissive = glm::vec3{0.0f};
  defaultMaterial.specular = glm::vec3{1.0f};
  defaultMaterial.shininess = 32.0;
  defaultMaterial.color = glm::vec3{0.5f};

  materials.push_back(defaultMaterial);

  loadAsset("./assets/Cube/glTF/Cube.gltf");
};

void Engine::loadAsset(const std::filesystem::path& path) {
  Assimp::Importer importer{};

  const aiScene* scene = importer.ReadFile(
      path.string().c_str(), aiProcess_Triangulate | aiProcess_FlipUVs |
                                 aiProcess_OptimizeMeshes |
                                 aiProcess_CalcTangentSpace);

  assert(scene != nullptr);

  Asset asset{};
  asset.path = path;

  std::vector<TextureCreateInfo> createInfos;
  std::unordered_map<std::string, ImageData> cache;

  processNode(asset, scene, scene->mRootNode, createInfos);

  std::vector<std::thread> threads;

  for (const TextureCreateInfo& createInfo : createInfos) {
    std::string p = createInfo.path.string();
    threads.push_back(std::thread([&, p]() {
      int height{}, width{};
      uint8_t* data = stbi_load(p.c_str(), &width, &height, 0, STBI_rgb_alpha);

      assert(data != nullptr);

      ImageData image{};
      image.data = data;
      image.height = static_cast<uint32_t>(height);
      image.width = static_cast<uint32_t>(width);

      cache[p.c_str()] = image;
    }));
  }

  for (auto& t : threads) {
    t.join();
  }

  for (const TextureCreateInfo& createInfo : createInfos) {
    ImageData image = cache[createInfo.path.string()];
    Texture& texture = textures[createInfo.textureIdx];
    vk::Format format = texture.type == TextureType::Normal
                            ? vk::Format::eR8G8B8A8Unorm
                            : vk::Format::eR8G8B8A8Srgb;
    texture.image = gpu.createTexture2D(
        image.data,
        vk::Extent2D{}.setWidth(image.width).setHeight(image.height), format);
    texture.sampler = gpu.createSampler(createInfo.sampler.toVkSampler(
        texture.image,
        gpu.physicalDeviceProperties.properties.limits.maxSamplerAnisotropy));
  }

  importer.FreeScene();
  assets.push_back(asset);
}

void Engine::loadMesh(Asset& asset,
                      const aiScene* scene,
                      const aiMesh* assimpMesh,
                      std::vector<TextureCreateInfo>& createInfos) {
  Mesh mesh{};

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  for (uint32_t j = 0; j < assimpMesh->mNumVertices; j++) {
    Vertex vertex{};

    vertex.position[0] = assimpMesh->mVertices[j].x;
    vertex.position[1] = assimpMesh->mVertices[j].y;
    vertex.position[2] = assimpMesh->mVertices[j].z;

    if (assimpMesh->HasNormals()) {
      vertex.normal[0] = assimpMesh->mNormals[j].x;
      vertex.normal[1] = assimpMesh->mNormals[j].y;
      vertex.normal[2] = assimpMesh->mNormals[j].z;

      vertex.tangent[0] = assimpMesh->mTangents[j].x;
      vertex.tangent[1] = assimpMesh->mTangents[j].y;
      vertex.tangent[2] = assimpMesh->mTangents[j].z;
      vertex.tangent[3] = 1.0f;

      glm::vec3 bitangent{assimpMesh->mBitangents[j].x,
                          assimpMesh->mBitangents[j].y,
                          assimpMesh->mBitangents[j].z};

      if (glm::dot(glm::cross(glm::vec3{vertex.tangent}, vertex.normal),
                   bitangent) < 0.0f) {
        vertex.tangent[3] = -1.0f;
      }
    }

    vertex.uv[0] = 0.0f;
    vertex.uv[1] = 1.0f;

    if (assimpMesh->HasTextureCoords(0)) {
      vertex.uv[0] = assimpMesh->mTextureCoords[0][j].x;
      vertex.uv[1] = assimpMesh->mTextureCoords[0][j].y;
    }

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

  for (uint32_t j = 0; j < assimpMesh->mNumFaces; j++) {
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

  mesh.indicesCount = static_cast<uint32_t>(indices.size());

  loadMaterial(asset, mesh, scene->mMaterials[assimpMesh->mMaterialIndex],
               createInfos);

  asset.meshes.push_back(static_cast<uint32_t>(meshes.size()));
  meshes.push_back(mesh);
}

void Engine::loadMaterial(Asset& asset,
                          Mesh& mesh,
                          const aiMaterial* assimpMaterial,
                          std::vector<TextureCreateInfo>& createInfos) {
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

  bool twoSided = false;
  if (assimpMaterial->Get(AI_MATKEY_TWOSIDED, twoSided) == aiReturn_SUCCESS) {
    if (twoSided) {
      material.cullMode = vk::CullModeFlagBits::eNone;
    }
  };

  aiString alphaModeStr;
  if (assimpMaterial->Get("$mat.gltf.alphaMode", 0, 0, alphaModeStr) ==
      aiReturn_SUCCESS) {
    std::string mode = alphaModeStr.C_Str();
    if (mode == "MASK") {
      material.alphaMode = AlphaMode::Mask;
      float alphaCutoff = 0.5f;
      if (assimpMaterial->Get("$mat.gltf.alphaCutoff", 0, 0, alphaCutoff) ==
          aiReturn_SUCCESS) {
        material.alphaCutoff = alphaCutoff;
      }
    }
  }

  float transmissionFactor = 0.0f;
  if (assimpMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmissionFactor) ==
      aiReturn_SUCCESS) {
    material.transmissionFactor = transmissionFactor;
  };

  float roughness = 1.0f;
  if (assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) ==
      aiReturn_SUCCESS) {
    material.roughness = roughness;
  };

  if (asset.path.filename() == "DamagedHelmet.gltf") {
    material.roughness = 0.5f;
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    auto [diffuse, diffuseSampler] =
        loadTexture(assimpMaterial, aiTextureType_DIFFUSE);
    material.diffuseTextureIdx = static_cast<uint32_t>(textures.size());
    createInfos.push_back({asset.path.parent_path().append(diffuse.path),
                           diffuseSampler, material.diffuseTextureIdx});
    textures.push_back(diffuse);
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
    auto [specular, specularSampler] =
        loadTexture(assimpMaterial, aiTextureType_SPECULAR);
    material.specularTextureIdx = static_cast<uint32_t>(textures.size());
    createInfos.push_back({asset.path.parent_path().append(specular.path),
                           specularSampler, material.specularTextureIdx});
    textures.push_back(specular);
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
    auto [normal, normalSampler] =
        loadTexture(assimpMaterial, aiTextureType_NORMALS);
    material.normalTextureIdx = static_cast<uint32_t>(textures.size());
    createInfos.push_back({asset.path.parent_path().append(normal.path),
                           normalSampler, material.normalTextureIdx});
    textures.push_back(normal);
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0) {
    auto [height, heightSampler] =
        loadTexture(assimpMaterial, aiTextureType_HEIGHT);
    material.heightTextureIdx = static_cast<uint32_t>(textures.size());
    createInfos.push_back({asset.path.parent_path().append(height.path),
                           heightSampler, material.heightTextureIdx});
    textures.push_back(height);
  }

  mesh.materialIdx = static_cast<uint32_t>(materials.size());
  materials.push_back(material);
}

void Engine::processNode(Asset& asset,
                         const aiScene* scene,
                         const aiNode* node,
                         std::vector<TextureCreateInfo>& createInfos) {
  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    loadMesh(asset, scene, scene->mMeshes[node->mMeshes[i]], createInfos);
  }

  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    processNode(asset, scene, node->mChildren[i], createInfos);
  }
};

Image Engine::loadCubemap(const std::string& type) {
  int height{}, width{};
  std::array<uint8_t*, 6> images{};

  std::thread threads[6];

  for (uint32_t i = 0; i < 6; i++) {
    threads[i] = std::thread{[&, i]() {
      std::string file = CUBEMAP_FILES[i];
      uint8_t* data = stbi_load(
          std::string{"./textures/" + type + "/" + file + ".png"}.c_str(),
          &width, &height, nullptr, STBI_rgb_alpha);
      assert(data != nullptr);
      images[i] = data;
    }};
  }

  for (uint32_t i = 0; i < 6; i++) {
    threads[i].join();
  }

  Image cubemap = gpu.createCubemapTexture(
      images, vk::Extent2D{}
                  .setWidth(static_cast<uint32_t>(width))
                  .setHeight(static_cast<uint32_t>(height)));

  for (const auto& data : images) {
    stbi_image_free(data);
  }

  return cubemap;
}

void Engine::prepareDescriptors() {
  Texture shadowMap{};
  shadowMap.sampler = shadowMapSampler;
  shadowMap.type = TextureType::Shadow;

  if (directionalLight.shadows) {
    directionalLight.shadowMapIdx = shadowMaps.size();
    shadowMap.image = gpu.createShadowMap();
    shadowMaps.push_back(shadowMap);
  }

  for (PointLight& light : pointLights) {
    if (light.shadows) {
      light.shadowMapIdx = shadowMaps.size();
      shadowMap.image = gpu.createShadowMap();
      shadowMaps.push_back(shadowMap);
    }
  }

  for (SpotLight& light : spotLights) {
    if (light.shadows) {
      light.shadowMapIdx = shadowMaps.size();
      shadowMap.image = gpu.createShadowMap();
      shadowMaps.push_back(shadowMap);
    }
  }

  transformUniform = gpu.createBuffer(&transform, sizeof(Transform),
                                      vk::BufferUsageFlagBits::eUniformBuffer);

  directionalLightUniform =
      gpu.createBuffer(&directionalLight, sizeof(DirectionalLight),
                       vk::BufferUsageFlagBits::eUniformBuffer);

  skylightUniform = gpu.createBuffer(&skylight, sizeof(Skylight),
                                     vk::BufferUsageFlagBits::eUniformBuffer);

  if (pointLights.size()) {
    pointLightsBuffer = gpu.createBuffer(
        pointLights.data(), sizeof(PointLight) * pointLights.size(),
        vk::BufferUsageFlagBits::eStorageBuffer);
  } else {
    pointLightsBuffer = gpu.createBuffer(
        sizeof(PointLight), vk::BufferUsageFlagBits::eStorageBuffer);
  }

  if (spotLights.size()) {
    spotLightsBuffer = gpu.createBuffer(
        spotLights.data(), sizeof(SpotLight) * spotLights.size(),
        vk::BufferUsageFlagBits::eStorageBuffer);
  } else {
    spotLightsBuffer = gpu.createBuffer(
        sizeof(SpotLight) * 1, vk::BufferUsageFlagBits::eStorageBuffer);
  }

  if (entities.size()) {
    entitiesBuffer =
        gpu.createBuffer(entities.data(), sizeof(Entity) * entities.size(),
                         vk::BufferUsageFlagBits::eStorageBuffer);
  } else {
    entitiesBuffer = gpu.createBuffer(sizeof(Entity),
                                      vk::BufferUsageFlagBits::eStorageBuffer);
  }

  if (materials.size()) {
    materialsBuffer =
        gpu.createBuffer(materials.data(), sizeof(Material) * materials.size(),
                         vk::BufferUsageFlagBits::eStorageBuffer);
  } else {
    materialsBuffer = gpu.createBuffer(sizeof(Material),
                                       vk::BufferUsageFlagBits::eStorageBuffer);
  }

  gpu.setUniformDescriptorSet(transformUniform,
                              gpu.mainPipeline.descriptorSets[0], 0);
  gpu.setUniformDescriptorSet(skylightUniform,
                              gpu.mainPipeline.descriptorSets[3], 0);
  gpu.setUniformDescriptorSet(directionalLightUniform,
                              gpu.mainPipeline.descriptorSets[3], 1);

  gpu.setStorageBufferDescriptorSet(pointLightsBuffer,
                                    gpu.mainPipeline.descriptorSets[3], 2);
  gpu.setStorageBufferDescriptorSet(spotLightsBuffer,
                                    gpu.mainPipeline.descriptorSets[3], 3);

  gpu.setStorageBufferDescriptorSet(entitiesBuffer,
                                    gpu.mainPipeline.descriptorSets[1], 0);
  gpu.setStorageBufferDescriptorSet(entitiesBuffer,
                                    gpu.shadowsPipeline.descriptorSets[0], 0);

  gpu.setStorageBufferDescriptorSet(materialsBuffer,
                                    gpu.mainPipeline.descriptorSets[1], 1);

  gpu.setTextureArrayDescriptorSet(textures, gpu.mainPipeline.descriptorSets[2],
                                   0);

  if (shadowMaps.size()) {
    gpu.setTextureArrayDescriptorSet(shadowMaps,
                                     gpu.mainPipeline.descriptorSets[2], 1);
  }

  gpu.setTextureArrayDescriptorSet(std::vector{skybox},
                                   gpu.mainPipeline.descriptorSets[2], 2);
  gpu.setTextureDescriptorSet(skybox, gpu.skyboxPipeline.descriptorSets[0], 0);
}

void Engine::loadSkybox() {
  Image cubemap = loadCubemap("skybox");

  vk::SamplerCreateInfo samplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
          .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
          .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(
              std::min(16.0f, gpu.physicalDeviceProperties.properties.limits
                                  .maxSamplerAnisotropy))
          .setCompareEnable(0);

  Texture tex{};
  tex.image = cubemap;
  tex.sampler = gpu.createSampler(samplerCreateInfo);
  tex.type = TextureType::Cube;

  skybox = tex;
}

void Engine::drawShadows(const ShadowPassFrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 gpu.shadowsPipeline.pipeline);

  gpu.commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, gpu.shadowsPipeline.layout, 0,
      gpu.shadowsPipeline.descriptorSets.size(),
      gpu.shadowsPipeline.descriptorSets.data(), 0, nullptr);

  std::vector<vk::DeviceSize> offsets = {0};

  vk::Extent2D extent =
      vk::Extent2D{}.setWidth(gpu.shadowSize).setHeight(gpu.shadowSize);

  vk::Viewport viewport = vk::Viewport{}
                              .setWidth(static_cast<float>(extent.width))
                              .setHeight(static_cast<float>(extent.height))
                              .setMaxDepth(1.0)
                              .setMinDepth(0.0)
                              .setX(0.0)
                              .setY(0.0);

  vk::Rect2D scissors = vk::Rect2D{}.setExtent(
      vk::Extent2D{}.setHeight(extent.height).setWidth(extent.width));

  gpu.commandBuffer.setViewport(0, 1, &viewport);
  gpu.commandBuffer.setScissor(0, 1, &scissors);
  gpu.commandBuffer.setCullMode(vk::CullModeFlagBits::eNone);
  gpu.commandBuffer.setDepthWriteEnable(1);

  vk::Bool32 enables[1] = {false};
  gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);

  ShadowPassFrameData data = frameData;

  for (uint32_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    Asset& asset = assets[entity.assetIdx];

    data.entityId = i;
    gpu.commandBuffer.pushConstants(gpu.shadowsPipeline.layout,
                                    vk::ShaderStageFlagBits::eVertex, 0,
                                    sizeof(ShadowPassFrameData), &data);

    for (const uint32_t meshIdx : asset.meshes) {
      drawEntityShadow(meshIdx);
    }
  }
}

void Engine::drawEntities(const MainPassFrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 gpu.mainPipeline.pipeline);

  gpu.commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, gpu.mainPipeline.layout, 0,
      gpu.mainPipeline.descriptorSets.size(),
      gpu.mainPipeline.descriptorSets.data(), 0, nullptr);

  std::vector<vk::DeviceSize> offsets = {0};

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);
  gpu.commandBuffer.setDepthWriteEnable(1);

  std::vector<std::array<uint32_t, 3>> transparent{};
  std::vector<std::array<uint32_t, 2>> opaque{};

  for (uint32_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    Asset& asset = assets[entity.assetIdx];

    for (const uint32_t meshIdx : asset.meshes) {
      Mesh& mesh = meshes[meshIdx];
      Material& material = materials[mesh.materialIdx];

      if (material.alphaMode != AlphaMode::Opaque) {
        float distance =
            glm::length2(camera.position - glm::vec3(entity.matrix[3]));
        transparent.push_back({static_cast<uint32_t>(i), meshIdx,
                               static_cast<uint32_t>(distance)});
      } else {
        opaque.push_back({static_cast<uint32_t>(i), meshIdx});
      }
    }
  }

  vk::Bool32 enables[1] = {false};
  gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);

  MainPassFrameData data = frameData;

  for (const auto& [entityIdx, meshIdx] : opaque) {
    data.entityId = entityIdx;
    data.materialId = meshes[meshIdx].materialIdx;

    gpu.commandBuffer.pushConstants(
        gpu.mainPipeline.layout,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, sizeof(MainPassFrameData), &data);

    drawEntity(meshIdx);
  }

  enables[0] = true;
  gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);

  std::sort(transparent.begin(), transparent.end(),
            [](const std::array<uint32_t, 3>& a,
               const std::array<uint32_t, 3>& b) { return a[2] > b[2]; });

  for (const auto& [entityIdx, meshIdx, distance] : transparent) {
    data.entityId = entityIdx;
    data.materialId = meshes[meshIdx].materialIdx;

    gpu.commandBuffer.pushConstants(
        gpu.mainPipeline.layout,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, sizeof(MainPassFrameData), &data);

    drawEntity(meshIdx);
  }
}

vk::SamplerCreateInfo AssimpSampler::toVkSampler(
    const Image& image,
    float maxSamplerAnisotropy) const {
  vk::SamplerCreateInfo samplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMaxLod(static_cast<float>(image.mipLevels))
          .setMinLod(0.0f)
          .setMipLodBias(0.0f)
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(std::min(16.0f, maxSamplerAnisotropy))
          .setCompareEnable(0);

  switch (mag) {
    case GLTFMagFilter::Linear:
      samplerCreateInfo.setMagFilter(vk::Filter::eLinear);
      break;
    case GLTFMagFilter::Nearest:
      samplerCreateInfo.setMagFilter(vk::Filter::eNearest);
      break;
    default:
      samplerCreateInfo.setMagFilter(vk::Filter::eLinear);
  }

  switch (min) {
    case GLTFMinFilter::Linear:
      samplerCreateInfo.setMinFilter(vk::Filter::eLinear);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
      break;
    case GLTFMinFilter::LinearMipmapLinear:
      samplerCreateInfo.setMinFilter(vk::Filter::eLinear);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
      break;
    case GLTFMinFilter::LinearMipmapNearest:
      samplerCreateInfo.setMinFilter(vk::Filter::eLinear);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
      break;
    case GLTFMinFilter::Nearest:
      samplerCreateInfo.setMinFilter(vk::Filter::eNearest);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
      break;
    case GLTFMinFilter::NearestMipmapLinear:
      samplerCreateInfo.setMinFilter(vk::Filter::eNearest);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
      break;
    case GLTFMinFilter::NearestMipmapNearest:
      samplerCreateInfo.setMinFilter(vk::Filter::eNearest);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
      break;
    default:
      samplerCreateInfo.setMinFilter(vk::Filter::eLinear);
      samplerCreateInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
  }

  switch (u) {
    case aiTextureMapMode_Wrap:
      samplerCreateInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
      break;
    case aiTextureMapMode_Clamp:
      samplerCreateInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
      break;
    case aiTextureMapMode_Mirror:
      samplerCreateInfo.setAddressModeU(
          vk::SamplerAddressMode::eMirroredRepeat);
      break;
    default:
      samplerCreateInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
  }

  switch (v) {
    case aiTextureMapMode_Wrap:
      samplerCreateInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
      break;
    case aiTextureMapMode_Clamp:
      samplerCreateInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
      break;
    case aiTextureMapMode_Mirror:
      samplerCreateInfo.setAddressModeV(
          vk::SamplerAddressMode::eMirroredRepeat);
      break;
    default:
      samplerCreateInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
  }

  return samplerCreateInfo;
}

void Engine::drawMesh(const uint32_t meshIdx) {
  vk::DeviceSize offsets[1] = {0};

  Mesh& mesh = meshes[meshIdx];

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);
  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);

  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}

void Engine::drawEntityShadow(const uint32_t meshIdx) {
  vk::DeviceSize offsets[1] = {0};
  Mesh& mesh = meshes[meshIdx];

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);
  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);

  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}

void Engine::drawEntity(const uint32_t meshIdx) {
  vk::DeviceSize offsets[1] = {0};

  Mesh& mesh = meshes[meshIdx];
  Material& material = materials[mesh.materialIdx];

  gpu.commandBuffer.setCullMode(material.cullMode);

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);
  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);
  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}

std::pair<Texture, AssimpSampler> Engine::loadTexture(
    const aiMaterial* material,
    const aiTextureType textureType) {
  Texture texture{};

  switch (textureType) {
    case aiTextureType_DIFFUSE:
      texture.type = TextureType::BaseColor;
      break;
    case aiTextureType_SPECULAR:
      texture.type = TextureType::Specular;
      break;
    case aiTextureType_NORMALS:
      texture.type = TextureType::Normal;
      break;
    case aiTextureType_HEIGHT:
      texture.type = TextureType::Height;
      break;
    default:
      texture.type = TextureType::BaseColor;
      break;
  }

  aiString texturePath;
  material->GetTexture(textureType, 0, &texturePath);

  texture.path = texturePath.C_Str();

  aiTextureMapMode mapModeU, mapModeV;
  GLTFMinFilter min;
  GLTFMagFilter mag;

  material->Get(AI_MATKEY_MAPPINGMODE_U(textureType, 0), mapModeU);
  material->Get(AI_MATKEY_MAPPINGMODE_V(textureType, 0), mapModeV);
  material->Get(AI_MATKEY_GLTF_MAPPINGFILTER_MAG(textureType, 0), min);
  material->Get(AI_MATKEY_GLTF_MAPPINGFILTER_MIN(textureType, 0), mag);

  return std::make_pair(texture, AssimpSampler{mapModeU, mapModeV, mag, min});
}
