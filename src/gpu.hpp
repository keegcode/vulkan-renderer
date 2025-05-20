#pragma once

#include <cstdio>
#include <glm/ext/matrix_float4x4.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include "VkBootstrap.h"
#include "display.hpp"
#include "vk_mem_alloc.h"

template <typename T>
void inline VKB_ASSERT(vkb::Result<T> vkbResult) {
  if (!vkbResult.has_value()) {
    printf("%s\n", vkbResult.error().message().c_str());
    DEBUG_BREAK();
  }
}

struct ShadowPassFrameData {
  glm::mat4 lightSpaceMatrix;
  glm::mat4 model;
};

struct MainPassFrameData {
  glm::vec3 cameraPos;
  uint32_t pointLights;
  uint32_t spotLights;
};

struct SkyboxFrameData {
  glm::mat4 matrix;
};

struct Vertex {
  float position[3];
  float clr[4];
  float uv[2];
  float normal[3];
  float tangent[3];
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
  uint32_t mipLevels;
  vk::ImageView view;
  vk::Extent3D extent;
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

struct ImageMemoryBarrierOptions {
  vk::Image image;
  vk::ImageLayout newLayout;
  vk::ImageLayout oldLayout = vk::ImageLayout::eUndefined;
  vk::AccessFlags2 srcAccessMask = vk::AccessFlagBits2::eNone;
  vk::AccessFlags2 dstAccessMask = vk::AccessFlagBits2::eNone;
  vk::PipelineStageFlags2 srcStageMask = vk::PipelineStageFlagBits2::eNone;
  vk::PipelineStageFlags2 dstStageMask = vk::PipelineStageFlagBits2::eNone;
  uint32_t layers = 1;
  uint32_t mipLevel = 0;
  uint32_t levelCount = 1;
};

struct DepthImageOptions {
  vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
  vk::ImageUsageFlagBits usage =
      vk::ImageUsageFlagBits::eDepthStencilAttachment;
  vk::Extent2D extent;
};

class GPU {
 public:
  uint32_t shadowSize;

  Display display;

  Pipeline entitiesPipeline;
  Pipeline skyboxPipeline;
  Pipeline shadowsPipeline;

  vkb::Instance instance;
  vk::detail::DispatchLoaderDynamic dld;

  vk::SampleCountFlagBits sampleCount;

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

  vk::DescriptorSetLayout uniformLayout;
  vk::DescriptorSetLayout textureLayout;
  vk::DescriptorSetLayout lightLayout;
  vk::DescriptorSetLayout storageBufferLayout;
  vk::DescriptorSetLayout skyboxLayout;
  vk::DescriptorSetLayout globalMapLayout;

  Image depthImage;
  Image multisampleImage;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  GPU(const Display& d, const uint32_t s);

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
  void createViewportAndScissors();
  void createPipelines();

  Descriptor createStorageBufferDescriptor(
      const vk::DescriptorSetLayout& layout) const;
  Descriptor createUniformDescriptor(
      const uint32_t count,
      const vk::DescriptorSetLayout& layout) const;
  Descriptor createTextureDescriptor(
      const uint32_t count,
      const vk::DescriptorSetLayout& layout) const;
  void setDescriptorUniformBuffer(const Descriptor& descriptor,
                                  const Buffer& src,
                                  uint32_t index,
                                  uint32_t binding) const;
  void setDescriptorStorageBuffer(const Descriptor& descriptor,
                                  const Buffer& src,
                                  uint32_t index,
                                  uint32_t binding) const;
  void setDescriptorImage(const Descriptor& descriptor,
                          const Image& src,
                          const vk::Sampler& sampler,
                          uint32_t index,
                          uint32_t binding) const;
  vk::DeviceSize getDescriptorBindingOffset(const Descriptor& descriptor,
                                            uint32_t binding) const;

  Buffer createBuffer(const vk::DeviceSize size,
                      const vk::Flags<vk::BufferUsageFlagBits> usage) const;

  Buffer createBuffer(const void* data,
                      const vk::DeviceSize size,
                      const vk::Flags<vk::BufferUsageFlagBits> usage) const;

  Buffer createBuffer(const vk::DeviceSize s,
                      const vk::Flags<vk::BufferUsageFlagBits> usage,
                      VmaMemoryUsage memoryUsage,
                      VmaAllocationCreateFlags createFlags) const;

  void copyBufferToImage(const Buffer& buffer,
                         const Image& image,
                         const vk::Extent2D& extent,
                         const uint32_t layers = 1) const;

  vk::DeviceAddress getBufferDeviceAddress(const Buffer& buffer) const;

  vk::CommandBuffer beginSingleSubmitCommand() const;
  void endSingleSubmitCommand(
      const vk::CommandBuffer& singleSubmitBuffer) const;

  Image createDepthImage(const DepthImageOptions& options);
  Image createTexture2D(const uint8_t* data,
                        const vk::Extent2D& extent,
                        const vk::Format format = vk::Format::eR8G8B8A8Srgb);
  Image createCubemapTexture(const std::array<uint8_t*, 6>& data,
                             const vk::Extent2D& extent);
  void createImages();
  void createMultiSampleImage();
  void generateMipmaps(const Image& image);

  vk::Sampler createSampler(const vk::SamplerCreateInfo& createInfo);

  void addImageMemoryBarrier(vk::CommandBuffer& cmdBuffer,
                             const ImageMemoryBarrierOptions& options);

  Shader loadShader(const std::string_view path,
                    vk::ShaderStageFlagBits stage) const;

  Pipeline createPipeline(
      const Shader& vertexShader,
      const Shader& fragmentShader,
      std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
      const uint32_t pushConstantSize) const;

  void createShadowPipeline();

  void waitForFence() const;
  int32_t acquireNextImage() const;
  void resetFence() const;
  void beginRecordingCommands();
  void beginMainPass(const uint32_t imageIndex);
  void beginShadowPass(const Image& shadowMapImage);
  void endRendering() const;
  void submit(const uint32_t imageIndex);

  void destroyPipeline(const Pipeline& pipeline) const;
  void destroyShader(const Shader& shader) const;
  void destroyDescriptor(const Descriptor& descriptor) const;
  void destroyBuffer(const Buffer& buffer) const;
  void destroyImage(const Image& image) const;
  void destroySampler(const vk::Sampler& sampler);
  void destroy() const;
};
