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

#include "vk-structs.hpp"
#include "vk-shader.hpp"
#include "sdl-display.hpp"

class VkEngine {
public:
  std::vector<Mesh> meshes;
  std::vector<Entity> entities;

  VertexShader vertexShader;
  FragmentShader fragmentShader;

  bool isRunning = true;

  void init(const Display& d, const Transform& transform, const std::string_view meshPath, const std::string_view texturePath);
  void createMesh(const std::string_view path);
  void drawFrame();
  void processInput();
  void destroy();
private:
  Display display;

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

  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commadBuffers;

  uint16_t MAX_CONCURRENT_FRAMES = 2;
  uint16_t frame = 0;
  bool shouldBeResized = false;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  vk::Pipeline graphicsPipeline;
  vk::PipelineLayout pipelineLayout;

  void createInstance();
  void pickPhysicalDevice();
  void pickDevice();
  void createAllocator();
  void createSwapchain();
  void createDepthImage();
  void createViewportAndScissors();
  void createQueue();
  void createSyncPrimitives();
  void createCommandPool();
  void createCommandBuffers();
  void createShaders(const Transform& transform, const std::string_view& texturePath);
  void createGraphicsPipeline();
};
