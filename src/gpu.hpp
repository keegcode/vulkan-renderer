#pragma once

#include <cstdio>
#include <glm/ext/matrix_float4x4.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
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
  uint32_t entityId;
};

struct MainPassFrameData {
  glm::vec3 cameraPos;
  uint32_t pointLights;
  uint32_t spotLights;
  uint32_t materialId;
  uint32_t entityId;
};

struct SkyboxPassFrameData {
  glm::mat4 matrix;
};

struct Vertex {
  glm::vec3 position;
  glm::vec4 clr;
  glm::vec2 uv;
  glm::vec3 normal;
  glm::vec4 tangent;
};

struct Buffer {
  vk::Buffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
  vk::DeviceSize size;
};

struct Image {
  vk::ImageView view;
  vk::Extent3D extent;
  VmaAllocation allocation;
  uint32_t mipLevels;
  vk::Image image;
};

enum class TextureType { BaseColor, Specular, Cube, Normal, Height, Shadow };

struct Texture {
  Image image;
  TextureType type;
  vk::Sampler sampler;
  std::string path;
};

struct Shader {
  vk::ShaderStageFlagBits stage;
  vk::ShaderModule module;
};

struct PipelineOptions {
  std::vector<Shader> shaders;
  std::vector<vk::DescriptorSetLayoutCreateInfo>&
      descriptorSetLayoutCreateInfos;
  const uint32_t pushConstantSize;
  const uint32_t colorAttachmentCount;
  const float depthConstantBias = 0.0;
  const float depthSlopeBias = 0.0;
  const vk::SampleCountFlagBits msaa = vk::SampleCountFlagBits::e1;
};

struct Pipeline {
  vk::Pipeline pipeline;
  vk::PipelineLayout layout;
  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
  std::vector<vk::DescriptorSet> descriptorSets;
  std::vector<Shader> shaders;
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
  static const uint32_t shadowSize = 2048;
  static_assert((shadowSize & (shadowSize - 1)) == 0,
                "Shadow size should be 2^n");

  Display display;

  vk::DescriptorPool descriptorPool;
  std::vector<vk::DescriptorSet> descriptorSets;

  Pipeline mainPipeline;
  Pipeline skyboxPipeline;
  Pipeline shadowsPipeline;

  vkb::Instance instance;
  vk::detail::DispatchLoaderDynamic dld;

  vk::SampleCountFlagBits sampleCount;

  vk::SurfaceKHR surface;
  vkb::PhysicalDevice physicalDevice;
  vk::PhysicalDeviceProperties2 physicalDeviceProperties;
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

  Image depthImage;
  Image multisampleImage;

  vk::Viewport viewport;
  vk::Rect2D scissors;

  GPU(const Display& d);

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
  void createDescriptorSets();
  void createViewportAndScissors();
  void createPipelines();

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

  Image createDepthImage(const DepthImageOptions& options) const;
  Image createShadowMap() const;

  Image createTexture2D(const uint8_t* data,
                        const vk::Extent2D& extent,
                        const vk::Format format = vk::Format::eR8G8B8A8Srgb);
  Image createCubemapTexture(const std::array<uint8_t*, 6>& data,
                             const vk::Extent2D& extent);
  void createImages();
  void createMultiSampleImage();
  void generateMipmaps(const Image& image);

  vk::Sampler createSampler(const vk::SamplerCreateInfo& createInfo) const;

  void addImageMemoryBarrier(vk::CommandBuffer& cmdBuffer,
                             const ImageMemoryBarrierOptions& options);

  Shader loadShader(const std::string_view path,
                    vk::ShaderStageFlagBits stage) const;

  void setUniformDescriptorSet(const Buffer& src,
                               const vk::DescriptorSet& set,
                               const uint32_t binding);
  void setTextureArrayDescriptorSet(const std::vector<Texture>& textures,
                                    const vk::DescriptorSet& set,
                                    const uint32_t binding);
  void setTextureDescriptorSet(const Texture& texture,
                               const vk::DescriptorSet& set,
                               const uint32_t binding);
  void setStorageBufferDescriptorSet(const Buffer& src,
                                     const vk::DescriptorSet& set,
                                     const uint32_t binding);
  void updateDescriptor(const vk::WriteDescriptorSet& write);

  Pipeline createPipeline(const PipelineOptions& options) const;

  void waitForFence() const;
  int32_t acquireNextImage() const;
  void resetFence() const;
  void beginRecordingCommands() const;
  void beginMainPass(const uint32_t imageIndex);
  void beginShadowPass(const Texture& texture) const;
  void submit(const uint32_t imageIndex);

  void destroyPipeline(const Pipeline& pipeline) const;
  void destroyShader(const Shader& shader) const;
  void destroyBuffer(const Buffer& buffer) const;
  void destroyImage(const Image& image) const;
  void destroySampler(const vk::Sampler& sampler) const;
  void destroy() const;
};
