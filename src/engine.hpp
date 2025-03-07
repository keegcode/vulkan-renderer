#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "VkBootstrap.h"

#include "descriptor.hpp"
#include "display.hpp"
#include "mesh.hpp"
#include "pipeline.hpp"
#include "scene.hpp"

class Engine {
 public:
  Projection projection;

  std::vector<Mesh> meshes;
  std::vector<Texture> textures;
  std::vector<Object> objects;
  std::vector<Material> materials;

  Pipeline pipeline;

  bool isRunning = true;

  void init(const Display& d);
  void setLight(const Light& light);
  void loadMesh(const std::string_view path);
  void loadTexture(const std::string_view path);

  void drawFrame(float deltaTime);
  void processInput(float deltaTime);
  void destroy();

 private:
  Display display;
  Camera camera;
  Light light;

  vkb::Instance instance;
  vk::detail::DispatchLoaderDynamic dld;
  vk::SurfaceKHR surface;
  vkb::PhysicalDevice physicalDevice;
  vk::PhysicalDeviceProperties2 physicalDeviceProperties;
  vk::PhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties;
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
  
  Buffer uniformBuffer;

  Descriptor uniformDescriptor;
  Descriptor imageSamplerDescriptor;
  
  vk::CommandPool commandPool;
  vk::CommandBuffer commandBuffer;

  bool shouldBeResized = false;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  vk::Sampler sampler;

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
  void createUniformBuffer();
  void createDescriptors();
  void createCommandPool();
  void createCommandBuffer();
  void createSampler();
  void createPipeline();
};
