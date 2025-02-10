#pragma once

#include <SDL.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#include "VkBootstrap.h"
#include "display.hpp"
#include "light.hpp"
#include "mesh.hpp"
#include "object.hpp"
#include "pipeline.hpp"
#include "scene.hpp"
#include "texture.hpp"

class Engine {
public:
  std::vector<Mesh> meshes;
  std::vector<Texture> textures;
  std::vector<Object> objects;

  std::vector<Pipeline> pipelines;

  bool isRunning = true;

  void init(const Display &d);

  void setProjection(const Projection &projection);
  void setLight(const glm::vec3 &pos, const glm::vec3 color,
                const float ambient);
  void addObject(const UniformBuffer &uniform, const uint32_t meshIdx,
                 const uint32_t textureIdx, const uint32_t pipelineIdx);
  void loadMesh(const std::string_view path);
  void loadTexture(const std::string_view path);

  void drawFrame(float deltaTime);
  void processInput(float deltaTime);
  void destroy();

private:
  Display display;
  Light light;
  Camera camera;
  Projection projection;
  Descriptor projectionDescriptor;

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

  std::vector<vk::Fence> fences;
  std::vector<vk::Semaphore> renderCompleteSemaphores;
  std::vector<vk::Semaphore> presentCompleteSemaphores;

  vk::DescriptorSetLayout projectionSetLayout;
  vk::DescriptorSetLayout textureSetLayout;
  vk::DescriptorSetLayout objectSetLayout;
  vk::DescriptorSetLayout lightSetLayout;

  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commadBuffers;

  uint16_t MAX_CONCURRENT_FRAMES = 3;
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
  void createDescriptorSetLayouts();
  void createCommandPool();
  void createCommandBuffers();
  void createSampler();
  void createPipelines();
};
