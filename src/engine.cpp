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

#include <algorithm>
#include <assimp/Importer.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/matrix.hpp>
#include <thread>
#include <vector>
#include <filesystem>
#include <unordered_map>

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
  createDescriptors();
  prepareUniformsAndDescriptors();
};

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
  gpu.commandBuffer.reset();
  
  gpu.beginRecordingCommands();
  
  gpu.beginShadowPass();

  transform.model = transform.model;
  transform.view =
      glm::lookAt(camera.position, camera.position + camera.front, camera.up);
  transform.projection = transform.projection;

  vmaCopyMemoryToAllocation(gpu.allocator, &transform, transform.uniform.allocation, 0, offsetof(Transform, uniform));

  ShadowPassFrameData shadowPassFrameData{};
  shadowPassFrameData.model = transform.model;
  shadowPassFrameData.lightSpaceMatrix = directionalLight.lightSpaceMatrix;

  drawShadows(shadowPassFrameData);

  gpu.commandBuffer.endRendering();

  gpu.beginMainPass(static_cast<uint32_t>(imageIndex));

  MainPassFrameData mainPassFrameData{};
  mainPassFrameData.spotLights = spotLights.size();
  mainPassFrameData.pointLights = pointLights.size();
  mainPassFrameData.cameraPos = camera.position;

  drawEntities(mainPassFrameData);

  SkyboxFrameData skyboxFrameData{};
  skyboxFrameData.matrix = transform.projection * glm::mat4{glm::mat3{transform.view}} * transform.model;

  drawSkybox(skyboxFrameData);

  gpu.commandBuffer.endRendering();
  gpu.commandBuffer.end();

  gpu.submit(static_cast<uint32_t>(imageIndex));
};

void Engine::drawSkybox(const SkyboxFrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 gpu.skyboxPipeline.pipeline);

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);

  gpu.commandBuffer.setDepthWriteEnable(0);

  gpu.commandBuffer.pushConstants(
      gpu.skyboxPipeline.layout,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
      sizeof(SkyboxFrameData), &frameData);

  std::vector<vk::DescriptorBufferBindingInfoEXT> sceneBidningInfo{
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT |
                    vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(skyboxDescriptor.address.deviceAddress),

  };

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
      vk::PipelineBindPoint::eGraphics, gpu.skyboxPipeline.layout, 0, 1,
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

void Engine::destroyTexture(const Texture& texture) {
  gpu.destroyImage(texture.image);
  gpu.destroySampler(texture.sampler);
};

void Engine::destroy() {
  gpu.device.waitIdle();

  gpu.destroyDescriptor(lightsDescriptor);
  gpu.destroyDescriptor(materialsDescriptor);
  gpu.destroyDescriptor(entitiesDescriptor);
  gpu.destroyDescriptor(texturesDescriptor);
  gpu.destroyDescriptor(skyboxDescriptor);
  gpu.destroyDescriptor(globalMapDescriptor);
  gpu.destroyDescriptor(transformDescriptor);

  for (Texture& texture : textures) {
    destroyTexture(texture);
  }

  gpu.destroySampler(shadowMap.sampler);
  destroyTexture(skybox);

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
  
  gpu.destroyBuffer(transform.uniform);
  gpu.destroyBuffer(directionalLight.uniform);

  gpu.destroySwapchainResources();

  gpu.destroy();
  display.destroy();
};

Image Engine::loadImage(const std::filesystem::path& path) {
  int height, width;
  uint8_t* data =
      stbi_load(path.string().c_str(), &width, &height, 0, STBI_rgb_alpha);

  assert(data != nullptr);

  Image texture = gpu.createTexture2D(
      data, vk::Extent2D{}.setWidth(width).setHeight(height));

  stbi_image_free(data);

  return texture;
}

void Engine::loadConfig(const EngineConfig& config) {
  std::vector<glm::vec3> lightPositions{};

  transform = config.transform;
  directionalLight = config.directionalLight;

  for (size_t i = 0; i < config.pointLights.size(); i++) {
    PointLight light = config.pointLights[i];
    lightPositions.push_back(light.position);
    pointLights.push_back(light);
  }

  for (size_t i = 0; i < config.spotLights.size(); i++) {
    SpotLight light = config.spotLights[i];
    lightPositions.push_back(light.position);
    spotLights.push_back(light);
  }

  for (const std::filesystem::path& path : config.assets) {
    loadAsset(path);
  }

  for (size_t i = 0; i < config.entities.size(); i++) {
    entities.push_back(config.entities[i]);
  }

  for (size_t i = 0; i < lightPositions.size(); i++) {
    Entity entity{};
    entity.matrix = glm::scale(
        glm::translate(glm::mat4{1.0f}, lightPositions[i] + glm::vec3{0.0f, 20.0f, 0.0f}), glm::vec3{1.0});
    entity.assetIdx = 0;
    entities.push_back(entity);
  }
}

void Engine::loadStatic() {
  vk::SamplerCreateInfo samplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMagFilter(vk::Filter::eNearest)
          .setMinFilter(vk::Filter::eNearest)
          .setMipmapMode(vk::SamplerMipmapMode::eNearest)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(
              std::min(16.0f, gpu.physicalDeviceProperties.properties.limits
                                  .maxSamplerAnisotropy))
          .setCompareEnable(0);

  Texture texture{};
  texture.image = loadImage("./textures/default.png");
  texture.type = TextureType::BaseColor;
  texture.sampler = gpu.createSampler(samplerCreateInfo);

  vk::SamplerCreateInfo shadowMapSamplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
          .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
          .setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
          .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(
              std::min(16.0f, gpu.physicalDeviceProperties.properties.limits
                                  .maxSamplerAnisotropy));

  shadowMap.image = gpu.shadowMapImage;
  shadowMap.sampler = gpu.createSampler(shadowMapSamplerCreateInfo);

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
      path.string().c_str(),
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes);

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
      int height, width;
      uint8_t* data = stbi_load(p.c_str(), &width, &height, 0, STBI_rgb_alpha);

      assert(data != nullptr);

      ImageData image{};
      image.data = data;
      image.height = height;
      image.width = width;

      cache[p.c_str()] = image;
    }));
  }

  for (auto& t : threads) {
    t.join();
  }

  for (const TextureCreateInfo& createInfo : createInfos) {
    ImageData image = cache[createInfo.path.string()];
    textures[createInfo.textureIdx].image = gpu.createTexture2D(
        image.data,
        vk::Extent2D{}.setWidth(image.width).setHeight(image.height));
    textures[createInfo.textureIdx].sampler =
        gpu.createSampler(extractGLTFSampler(
            createInfo.mapModeU, createInfo.mapModeV, createInfo.magFilter,
            createInfo.minFilter, textures[createInfo.textureIdx].image));
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

  loadMaterial(asset, mesh, scene->mMaterials[assimpMesh->mMaterialIndex],
               createInfos);

  asset.meshes.push_back(meshes.size());
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
    material.roughness = 1.0f;
  };

  if (asset.path == "./assets/DamagedHelmet/glTF/DamagedHelmet.gltf") {
    material.roughness = 0.7f;
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
    aiString diffuseMapPath;
    assimpMaterial->GetTexture(aiTextureType::aiTextureType_BASE_COLOR, 0,
                               &diffuseMapPath);
    Texture diffuseMap{};
    diffuseMap.type = TextureType::BaseColor;
    material.diffuseTextureIdx = textures.size();

    aiTextureMapMode mapModeU, mapModeV;
    GLTFMinFilter min;
    GLTFMagFilter mag;

    assimpMaterial->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_BASE_COLOR, 0),
                        mapModeU);
    assimpMaterial->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_BASE_COLOR, 0),
                        mapModeV);
    assimpMaterial->Get(
        AI_MATKEY_GLTF_MAPPINGFILTER_MAG(aiTextureType_BASE_COLOR, 0), mag);
    assimpMaterial->Get(
        AI_MATKEY_GLTF_MAPPINGFILTER_MIN(aiTextureType_BASE_COLOR, 0), min);

    createInfos.push_back(
        {asset.path.parent_path().append(diffuseMapPath.C_Str()),
         extractGLTFSampler(mapModeU, mapModeV, mag, min, diffuseMap.image),
         material.diffuseTextureIdx, mapModeU, mapModeV, mag, min});

    textures.push_back(diffuseMap);
  }

  if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
    aiString specularMapPath;
    assimpMaterial->GetTexture(aiTextureType::aiTextureType_SPECULAR, 0,
                               &specularMapPath);
    Texture specularMap{};
    specularMap.type = TextureType::Specular;
    material.specularTextureIdx = textures.size();

    aiTextureMapMode mapModeU, mapModeV;
    GLTFMinFilter min;
    GLTFMagFilter mag;

    assimpMaterial->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_BASE_COLOR, 0),
                        mapModeU);
    assimpMaterial->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_BASE_COLOR, 0),
                        mapModeV);
    assimpMaterial->Get(
        AI_MATKEY_GLTF_MAPPINGFILTER_MAG(aiTextureType_BASE_COLOR, 0), min);
    assimpMaterial->Get(
        AI_MATKEY_GLTF_MAPPINGFILTER_MIN(aiTextureType_BASE_COLOR, 0), mag);

    createInfos.push_back(
        {asset.path.parent_path().append(specularMapPath.C_Str()),
         extractGLTFSampler(mapModeU, mapModeV, mag, min, specularMap.image),
         material.specularTextureIdx, mapModeU, mapModeV, mag, min});

    textures.push_back(specularMap);
  }

  mesh.materialIdx = materials.size();
  materials.push_back(material);
}

void Engine::processNode(Asset& asset,
                         const aiScene* scene,
                         const aiNode* node,
                         std::vector<TextureCreateInfo>& createInfos) {
  for (size_t i = 0; i < node->mNumMeshes; i++) {
    loadMesh(asset, scene, scene->mMeshes[node->mMeshes[i]], createInfos);
  }

  for (size_t i = 0; i < node->mNumChildren; i++) {
    processNode(asset, scene, node->mChildren[i], createInfos);
  }
};

Image Engine::loadCubemap(const std::string& type) {
  int height, width;
  std::array<uint8_t*, 6> images{};

  std::thread threads[6];

  for (size_t i = 0; i < 6; i++) {
    threads[i] = std::thread{[&, i]() {
      std::string file = CUBEMAP_FILES[i];
      uint8_t* data = stbi_load(
          std::string{"./textures/" + type + "/" + file + ".png"}.c_str(),
          &width, &height, nullptr, STBI_rgb_alpha);
      assert(data != nullptr);
      images[i] = data;
    }};
  }

  for (size_t i = 0; i < 6; i++) {
    threads[i].join();
  }

  Image cubemap = gpu.createCubemapTexture(
      images, vk::Extent2D{}.setWidth(width).setHeight(height));

  for (const auto& data : images) {
    stbi_image_free(data);
  }

  return cubemap;
}

void Engine::createDescriptors() {
  skyboxDescriptor = gpu.createTextureDescriptor(1, gpu.skyboxLayout);
  globalMapDescriptor = gpu.createTextureDescriptor(1, gpu.globalMapLayout);
  transformDescriptor = gpu.createUniformDescriptor(
      1, 
      gpu.uniformLayout
  );
  lightsDescriptor = gpu.createUniformDescriptor(
      spotLights.size() + pointLights.size() + 1, gpu.lightLayout);
  entitiesDescriptor =
      gpu.createUniformDescriptor(entities.size(), gpu.uniformLayout);
  materialsDescriptor =
      gpu.createUniformDescriptor(materials.size(), gpu.uniformLayout);
  texturesDescriptor =
      gpu.createTextureDescriptor(materials.size(), gpu.textureLayout);
}

void Engine::prepareUniformsAndDescriptors() {
  gpu.setDescriptorImage(skyboxDescriptor, skybox.image, skybox.sampler, 0, 0);

  gpu.setDescriptorImage(globalMapDescriptor, shadowMap.image,
                         shadowMap.sampler, 0, 0);
  gpu.setDescriptorImage(globalMapDescriptor, skybox.image, skybox.sampler, 0,
                         1);

  transform.uniform =
      gpu.createBuffer(&transform, offsetof(Transform, uniform),
                       vk::BufferUsageFlagBits::eUniformBuffer |
                           vk::BufferUsageFlagBits::eShaderDeviceAddress);

  gpu.setDescriptorUniformBuffer(transformDescriptor, transform.uniform, 0,
                                 0);

  directionalLight.uniform =
      gpu.createBuffer(&directionalLight, offsetof(DirectionalLight, uniform),
                       vk::BufferUsageFlagBits::eUniformBuffer |
                           vk::BufferUsageFlagBits::eShaderDeviceAddress);

  gpu.setDescriptorUniformBuffer(lightsDescriptor, directionalLight.uniform, 0,
                                 0);

    for (size_t i = 0; i < pointLights.size(); i++) {
    PointLight light = pointLights[i];

    light.uniform =
        gpu.createBuffer(&light, offsetof(PointLight, uniform),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);

    gpu.setDescriptorUniformBuffer(lightsDescriptor, light.uniform, i, 1);
  }

  for (size_t i = 0; i < spotLights.size(); i++) {
    SpotLight light = spotLights[i];

    light.uniform =
        gpu.createBuffer(&light, offsetof(SpotLight, uniform),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);

    gpu.setDescriptorUniformBuffer(lightsDescriptor, light.uniform, i, 2);
  }

  for (size_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    entity.uniform =
        gpu.createBuffer(&entity, sizeof(glm::mat4),
                         vk::BufferUsageFlagBits::eUniformBuffer |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress);
    gpu.setDescriptorUniformBuffer(entitiesDescriptor, entity.uniform, i, 0);
  }

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
                           diffuseMap.sampler, i, 0);

    gpu.setDescriptorImage(texturesDescriptor, specularMap.image,
                           specularMap.sampler, i, 1);
  }
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

  std::vector<vk::DeviceSize> offsets = {0};

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);

  gpu.commandBuffer.pushConstants(
      gpu.shadowsPipeline.layout,
      vk::ShaderStageFlagBits::eVertex, 0,
      sizeof(ShadowPassFrameData), &frameData);

  for (size_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    Asset& asset = assets[entity.assetIdx];
    for (const uint32_t meshIdx : asset.meshes) {
      drawEntityShadow(i, meshIdx);
    }
  }
}

void Engine::drawEntities(const MainPassFrameData& frameData) {
  gpu.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 gpu.entitiesPipeline.pipeline);

  std::vector<vk::DeviceSize> offsets = {0};

  gpu.commandBuffer.setViewport(0, 1, &gpu.viewport);
  gpu.commandBuffer.setScissor(0, 1, &gpu.scissors);
  gpu.commandBuffer.setDepthWriteEnable(1);

  gpu.commandBuffer.pushConstants(
      gpu.entitiesPipeline.layout,
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
      sizeof(MainPassFrameData), &frameData);

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
          .setAddress(globalMapDescriptor.address.deviceAddress),
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(transformDescriptor.address.deviceAddress),
  };

  gpu.commandBuffer.bindDescriptorBuffersEXT(sceneBidningInfo, gpu.dld);

  std::vector<std::array<uint32_t, 3>> transparent{};
  std::vector<std::array<uint32_t, 2>> opaque{};

  for (size_t i = 0; i < entities.size(); i++) {
    Entity& entity = entities[i];
    Asset& asset = assets[entity.assetIdx];
    for (const uint32_t meshIdx : asset.meshes) {
      Mesh& mesh = meshes[meshIdx];
      Material& material = materials[mesh.materialIdx];

      if (material.alphaMode != AlphaMode::Opaque) {
        uint32_t distance =
            glm::length2(camera.position - glm::vec3(entity.matrix[3]));
        transparent.push_back({static_cast<uint32_t>(i), meshIdx, distance});
      } else {
        opaque.push_back({static_cast<uint32_t>(i), meshIdx});
      }
    }
  }

  vk::Bool32 enables[1] = {false};
  gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);

  for (const auto& [entityIdx, meshIdx] : opaque) {
    drawEntity(entityIdx, meshIdx);
  }

  enables[0] = true;
  gpu.commandBuffer.setColorBlendEnableEXT(0, 1, enables, gpu.dld);

  std::sort(transparent.begin(), transparent.end(),
            [](const std::array<uint32_t, 3>& a,
               const std::array<uint32_t, 3>& b) { return a[2] > b[2]; });

  for (const auto& object : transparent) {
    drawEntity(object[0], object[1]);
  }
}

vk::SamplerCreateInfo Engine::extractGLTFSampler(const aiTextureMapMode u,
                                                 const aiTextureMapMode v,
                                                 const GLTFMagFilter mag,
                                                 const GLTFMinFilter min,
                                                 const Image& image) const {
  vk::SamplerCreateInfo samplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMaxLod(image.mipLevels)
          .setMinLod(0.0f)
          .setMipLodBias(0.0f)
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(1)
          .setMaxAnisotropy(
              std::min(16.0f, gpu.physicalDeviceProperties.properties.limits
                                  .maxSamplerAnisotropy))
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

void Engine::drawEntityShadow(const uint32_t entityIdx, const uint32_t meshIdx) {
  vk::DeviceSize offsets[1] = {0};
  Mesh& mesh = meshes[meshIdx];

  std::vector<vk::DescriptorBufferBindingInfoEXT> sceneBidningInfo{
      vk::DescriptorBufferBindingInfoEXT{}
          .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
          .setAddress(entitiesDescriptor.address.deviceAddress)};

  gpu.commandBuffer.bindDescriptorBuffersEXT(sceneBidningInfo, gpu.dld);

  std::vector<uint32_t> descriptorIndices{0};
  std::vector<vk::DeviceSize> descriptorOffsets{entitiesDescriptor.size * entityIdx};

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);
  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);

  gpu.commandBuffer.setDescriptorBufferOffsetsEXT(
      vk::PipelineBindPoint::eGraphics, gpu.shadowsPipeline.layout, 0, 1,
      descriptorIndices.data(), descriptorOffsets.data(), gpu.dld);

  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}

void Engine::drawEntity(const uint32_t entityIdx, const uint32_t meshIdx) {
  vk::DeviceSize offsets[1] = {0};

  Mesh& mesh = meshes[meshIdx];
  Material& material = materials[mesh.materialIdx];

  gpu.commandBuffer.setCullMode(material.cullMode);

  gpu.commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);

  gpu.commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                    vk::IndexType::eUint32);

  std::vector<uint32_t> descriptorIndices{0, 1, 2, 3, 4, 5};

  std::vector<vk::DeviceSize> descriptorOffsets{
      texturesDescriptor.size * mesh.materialIdx,
      materialsDescriptor.size * mesh.materialIdx, 
      0,
      entitiesDescriptor.size * entityIdx, 
      0, 
      0
  };

  gpu.commandBuffer.setDescriptorBufferOffsetsEXT(
      vk::PipelineBindPoint::eGraphics, gpu.entitiesPipeline.layout, 0, 6,
      descriptorIndices.data(), descriptorOffsets.data(), gpu.dld);

  gpu.commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
}
