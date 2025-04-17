#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include "VkBootstrap.h"
#include "display.hpp"
#include "vk_mem_alloc.h"

struct FrameData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 perspective;
  alignas(16) glm::vec3 cameraPos;
  uint32_t pointLights;
  uint32_t spotLights;
};

struct Vertex {
  float position[3];
  float clr[3];
  float uv[2];
  float normals[3];
};

struct Buffer {
  vk::Buffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
  vk::DeviceSize size;
};

struct Descriptor {
  Buffer buffer;
  vk::DescriptorSetLayout layout;
  vk::DeviceSize size;
  vk::DeviceOrHostAddressConstKHR address;
  vk::DescriptorType type;
};

struct Image {
  vk::Image image;
  vk::ImageView view;
  vk::Extent3D extent;
  vk::ImageLayout layout = vk::ImageLayout::eUndefined;
  VmaAllocation allocation;
};

struct Shader {
  vk::ShaderModule module;
  vk::ShaderStageFlagBits stage;
};

struct Pipeline {
  Shader vertexShader;
  Shader fragmentShader;
  vk::Pipeline pipeline;
  vk::PipelineLayout layout;
};

class GPU {
 public:
  Display display;

  vkb::Instance instance;
  vk::detail::DispatchLoaderDynamic dld;

  vk::SurfaceKHR surface;
  vkb::PhysicalDevice physicalDevice;
  vk::PhysicalDeviceProperties2 physicalDeviceProperties;
  vk::PhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties;
  vk::SurfaceCapabilitiesKHR capabilities;
  vkb::Device vkbDevice;
  vk::Device device;

  vkb::Swapchain vkbSwapchain;
  vk::SwapchainKHR swapchain;

  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;

  VmaAllocator allocator;

  vk::Queue queue;
  uint32_t queueIndex;

  vk::Fence fence;
  vk::Semaphore renderCompleteSemaphore;
  vk::Semaphore presentCompleteSemaphore;

  vk::CommandPool commandPool;
  vk::CommandBuffer commandBuffer;

  vk::Sampler diffuseSampler;
  vk::Sampler specularSampler;
  vk::Sampler skyboxSampler;

  vk::DescriptorSetLayout uniformLayout;
  vk::DescriptorSetLayout textureLayout;
  vk::DescriptorSetLayout lightLayout;
  vk::DescriptorSetLayout storageBufferLayout;
  vk::DescriptorSetLayout skyboxLayout;

  Image depthImage;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  GPU(const Display& display);

  void createInstance();
  void pickPhysicalDevice();
  void pickDevice();
  void createSwapchain();
  void rebuiltSwapchain();
  void destroySwapchainResources();
  void createAllocator();
  void createQueue();
  void createSyncPrimitives();
  void createCommandPool();
  void createCommandBuffer();
  void createDescriptorSetLayouts();
  void createSampler();
  void createViewportAndScissors();

  Descriptor createStorageBufferDescriptor(
      const vk::DescriptorSetLayout& layout);
  Descriptor createUniformDescriptor(const uint32_t count,
                                     const vk::DescriptorSetLayout& layout);
  Descriptor createTextureDescriptor(const uint32_t count,
                                     const vk::DescriptorSetLayout& layout);
  void setDescriptorUniformBuffer(const Descriptor& descriptor,
                                  const Buffer& src,
                                  uint32_t index,
                                  uint32_t binding);
  void setDescriptorStorageBuffer(const Descriptor& descriptor,
                                  const Buffer& src,
                                  uint32_t index,
                                  uint32_t binding);
  void setDescriptorImage(const Descriptor& descriptor,
                          const Image& src,
                          const vk::Sampler& sampler,
                          uint32_t index,
                          uint32_t binding);
  vk::DeviceSize getDescriptorBindingOffset(const Descriptor& descriptor,
                                            uint32_t binding);

  Buffer createBuffer(const vk::DeviceSize size,
                      const vk::Flags<vk::BufferUsageFlagBits> usage);

  Buffer createBuffer(const void* data,
                      const vk::DeviceSize size,
                      const vk::Flags<vk::BufferUsageFlagBits> usage);

  Buffer createBuffer(const vk::DeviceSize s,
                      const vk::Flags<vk::BufferUsageFlagBits> usage,
                      VmaMemoryUsage memoryUsage,
                      VmaAllocationCreateFlags createFlags);

  void copyBufferToImage(const Buffer& buffer,
                         const Image& image,
                         const vk::Extent2D& extent, 
                         const uint32_t layers = 1);

  vk::DeviceAddress getBufferDeviceAddress(const Buffer& buffer);

  vk::CommandBuffer beginSingleSubmitCommand();
  void endSingleSubmitCommand(const vk::CommandBuffer& singleSubmitBuffer);

  Image createTexture2D(const uint8_t* data, const vk::Extent2D& extent);
  Image createCubemapTexture(const std::array<uint8_t*, 6>& data, const vk::Extent2D& extent);
  void createDepthImage();

  vk::ImageLayout transitionImageLayout(const Image& image,
                                        const vk::ImageLayout& newLayout, const uint32_t layers = 1);

  Shader loadShader(const std::string_view path, vk::ShaderStageFlagBits stage);

  Pipeline createPipeline(
      const Shader& vertexShader,
      const Shader& fragmentShader,
      std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);

  void waitForFence();
  int32_t acquireNextImage();
  void resetFence();
  void beginRendering(uint32_t imageIndex);

  void destroyPipeline(const Pipeline& pipeline);
  void destroyShader(const Shader& shader);
  void destroyDescriptor(const Descriptor& descriptor);
  void destroyBuffer(const Buffer& buffer);
  void destroyImage(const Image& image);
  void destroy();
};
