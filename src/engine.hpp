#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "VkBootstrap.h"

#include "display.hpp"
#include "pipeline.hpp"
#include "scene.hpp"

struct SetTexture {
  std::string texture;
  std::string diffuseMap;
  std::string specularMap;
};

struct EngineState {
  ProjectionProperties projection;
  LightProperties light;
  std::vector<Entity> entities;
  std::vector<MaterialProperties> materials;
  std::vector<std::string> meshes;
  std::vector<SetTexture> textures;
};

class Engine {
 public:
  Pipeline pipeline;

  bool isRunning = true;

  void init(const Display& d, const EngineState& state);

  void drawFrame(float deltaTime);
  void processInput(float deltaTime);
  void destroy();

 private:
  Display display;
  Scene scene;

  vkb::Instance instance;
  vk::detail::DispatchLoaderDynamic dld;
  vk::SurfaceKHR surface;
  vkb::PhysicalDevice physicalDevice;
  vk::PhysicalDeviceProperties2 physicalDeviceProperties;
  vk::PhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties;
  vk::SurfaceCapabilitiesKHR capabilities;
  vkb::Device device;

  vk::Queue queue;
  uint32_t queueIndex;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;

  Image depthImage;

  VmaAllocator allocator;

  vk::Fence fence;
  vk::Semaphore renderCompleteSemaphore;
  vk::Semaphore presentCompleteSemaphore;

  vk::DescriptorSetLayout imageSamplerLayout;
  vk::DescriptorSetLayout uniformLayout;

  vk::CommandPool commandPool;
  vk::CommandBuffer commandBuffer;

  bool shouldBeResized = false;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  vk::Sampler textureSampler;
  vk::Sampler diffuseSampler;
  vk::Sampler specularSampler;

  void createInstance();
  void pickPhysicalDevice();
  void pickDevice();
  void createAllocator();
  void createSwapchain();
  void rebuiltSwapchain();
  void destroySwapchainResources();
  void createDepthImage();
  void createViewportAndScissors();
  void createQueue();
  void createSyncPrimitives();
  void createDescriptorSetLayouts();
  void createCommandPool();
  void createCommandBuffer();
  void createSampler();
  void createPipeline();
  void loadScene(const EngineState& state);

  void loadMesh(const std::string& path);
  Image loadImage(const std::string& path);
};
