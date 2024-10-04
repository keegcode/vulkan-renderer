#pragma once

#include <SDL.h>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <VkBootstrap.h>
#include <deque>
#include <functional>

#include "vk-pipeline.hpp"
#include "sdl-display.hpp"
#include "scene.hpp"
#include "object.hpp"
#include "mesh.hpp"
#include "texture.hpp"

class VkEngine {
public:
  std::vector<Mesh> meshes;
  std::vector<Texture> textures;
  std::vector<Object> objects;
  
  std::vector<Pipeline> pipelines;

  bool isRunning = true;

  void init(const Display& d);
  
  void setProjection(const Projection& projection);
  void addObject(const UniformBuffer& uniform, const uint32_t textureIdx, const uint32_t meshIdx, const uint32_t pipelineIdx);
  void loadMesh(const std::string_view path);
  void loadTexture(const std::string_view path);

  void drawFrame(float deltaTime);
  void processInput(float deltaTime);
  void destroy();
private:
  Display display;
  Projection projection;
  Camera camera;

  vkb::Instance instance;
  vk::SurfaceKHR surface;
  vkb::PhysicalDevice physicalDevice;
  vkb::Device device;

  vk::Queue queue;
  uint32_t queueIndex;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;

  Image depthImage;

  VmaAllocator allocator;
  std::deque<std::function<void()>> deleteQueue;

  std::vector<vk::Fence> fences;
  std::vector<vk::Semaphore> renderCompleteSemaphores;
  std::vector<vk::Semaphore> presentCompleteSemaphores;

  vk::DescriptorPool descriptorPool;
  vk::DescriptorSetLayout textureSetLayout;
  vk::DescriptorSetLayout objectSetLayout;

  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commadBuffers;

  uint16_t MAX_CONCURRENT_FRAMES = 2;
  uint16_t frame = 0;
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
  void createDescriptorPool();
  void createCommandPool();
  void createCommandBuffers();
  void createSampler();
  void createPipelines();
};
