#include "gpu.hpp"
#include <vulkan/vulkan_core.h>
#include <cassert>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "display.hpp"
#include "stb_image.h"
#include "utils.hpp"

GPU::GPU(const Display& d) : display{d} {
  createInstance();
  pickPhysicalDevice();
  pickDevice();
  createSwapchain();
  createAllocator();
  createQueue();
  createSyncPrimitives();
  createCommandPool();
  createCommandBuffer();
  createDescriptorSetLayouts();
  createImages();
  createViewportAndScissors();
  createPipelines();
};

void GPU::createInstance() {
  vkb::Result<vkb::Instance> instanceResult =
      vkb::InstanceBuilder{}
          .set_app_name("VkRenderer")
          .require_api_version(1, 3)
          .enable_extensions(display.vulkanExtensions)
          //.enable_validation_layers(true)
          //.use_default_debug_messenger()
          .build();

  VKB_ASSERT(instanceResult);

  instance = instanceResult.value();
  dld.init(instance.instance, instance.fp_vkGetInstanceProcAddr);
};

void GPU::setUniformDescriptorSet(
  const Buffer& src,
  const vk::DescriptorSet& set,
  const uint32_t binding
) {
  vk::DescriptorBufferInfo bufferInfo = vk::DescriptorBufferInfo{}
    .setBuffer(src.buffer)
    .setRange(src.size)
    .setOffset(0);

  vk::WriteDescriptorSet write = vk::WriteDescriptorSet{}
    .setDstSet(set)
    .setDstBinding(binding)
    .setBufferInfo(bufferInfo)
    .setDescriptorCount(1)
    .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  updateDescriptor(write);
}

void GPU::setTextureArrayDescriptorSet(
  const std::vector<Texture>& textures,
  const vk::DescriptorSet& set,
  const uint32_t binding
) {
  std::vector<vk::DescriptorImageInfo> imageInfos{};

  for (const Texture& texture : textures) {
    imageInfos.push_back(
      vk::DescriptorImageInfo{}
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImageView(texture.image.view)
        .setSampler(texture.sampler)
    );
  }

  vk::WriteDescriptorSet write = vk::WriteDescriptorSet{}
    .setDstSet(set)
    .setDstBinding(binding)
    .setImageInfo(imageInfos)
    .setDescriptorCount(imageInfos.size())
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler);

  updateDescriptor(write);
}

void GPU::setTextureDescriptorSet(
  const Texture& texture,
  const vk::DescriptorSet& set,
  const uint32_t binding
) {
  vk::DescriptorImageInfo imageInfo = vk::DescriptorImageInfo{}
    .setSampler(texture.sampler)
    .setImageView(texture.image.view)
    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  vk::WriteDescriptorSet write = vk::WriteDescriptorSet{}
    .setDstSet(set)
    .setDstBinding(binding)
    .setImageInfo(imageInfo)
    .setDescriptorCount(1)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler);

  updateDescriptor(write);
}

void GPU::setStorageBufferDescriptorSet(
  const Buffer& src,
  const vk::DescriptorSet& set,
  const uint32_t binding
) {
  vk::DescriptorBufferInfo bufferInfo = vk::DescriptorBufferInfo{}
    .setRange(src.size)
    .setOffset(0)
    .setBuffer(src.buffer);

  vk::WriteDescriptorSet write = vk::WriteDescriptorSet{}
    .setDstSet(set)
    .setDstBinding(binding)
    .setBufferInfo(bufferInfo)
    .setDescriptorCount(1)
    .setDescriptorType(vk::DescriptorType::eStorageBuffer);
  
  updateDescriptor(write);
}

void GPU::updateDescriptor(const vk::WriteDescriptorSet& write) {
  device.updateDescriptorSets(1, &write, 0, nullptr);
}

void GPU::destroy() const {
  destroyPipeline(mainPipeline);
  destroyPipeline(skyboxPipeline);
  destroyPipeline(shadowsPipeline);

  device.destroyDescriptorPool(descriptorPool);
  device.destroyCommandPool(commandPool);

  device.destroyFence(fence);
  device.destroySemaphore(renderCompleteSemaphore);
  device.destroySemaphore(presentCompleteSemaphore);

  vkb::destroy_swapchain(vkbSwapchain);

  vmaDestroyAllocator(allocator);

  vkb::destroy_surface(instance, surface);
  vkb::destroy_device(vkbDevice);
  vkb::destroy_instance(instance);
}

void GPU::pickPhysicalDevice() {
  surface = display.createVulkanSurface(instance.instance);

  std::vector<const char*> extensions = {
      vk::EXTDescriptorIndexingExtensionName,
      vk::EXTExtendedDynamicState3ExtensionName,
      vk::EXTScalarBlockLayoutExtensionName};

  vk::PhysicalDeviceFeatures features =
      vk::PhysicalDeviceFeatures{}.setSampleRateShading(1).setSamplerAnisotropy(
          1);

  vkb::Result<vkb::PhysicalDevice> physicalDeviceResult =
      vkb::PhysicalDeviceSelector{instance}
          .set_surface(surface)
          .set_minimum_version(1, 3)
          .require_present(true)
          .add_required_extensions(extensions)
          .set_required_features(features)
          .add_required_extension_features(
              vk::PhysicalDeviceDynamicRenderingFeatures{}.setDynamicRendering(
                  1))
          .add_required_extension_features(
              vk::PhysicalDeviceSynchronization2Features{}.setSynchronization2(
                  1))
          .add_required_extension_features(
              vk::PhysicalDeviceDescriptorIndexingFeaturesEXT{}
                  .setShaderSampledImageArrayNonUniformIndexing(1)
                  .setRuntimeDescriptorArray(1))
          .add_required_extension_features(
              vk::PhysicalDeviceBufferDeviceAddressFeatures{}
                  .setBufferDeviceAddress(1))
          .add_required_extension_features(
              vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT{}
                  .setExtendedDynamicState3ColorBlendEnable(1))
          .add_required_extension_features(
              vk::PhysicalDeviceScalarBlockLayoutFeatures{}
                  .setScalarBlockLayout(1))
          .select();

  VKB_ASSERT(physicalDeviceResult);

  physicalDevice = physicalDeviceResult.value();

  vk::PhysicalDevice pd = vk::PhysicalDevice{physicalDevice};

  pd.getProperties2(&physicalDeviceProperties);

  capabilities = pd.getSurfaceCapabilitiesKHR(physicalDevice.surface);
  sampleCount = vk::SampleCountFlagBits::e4;
};

void GPU::pickDevice() {
  vkb::Result<vkb::Device> deviceResult =
      vkb::DeviceBuilder{physicalDevice}.build();

  VKB_ASSERT(deviceResult);

  vkbDevice = deviceResult.value();
  device = vk::Device{vkbDevice.device};
};

void GPU::createSwapchain() {
  int32_t w{}, h{};
  SDL_GetWindowSize(display.window, &w, &h);

  vk::Extent2D extent = vk::Extent2D{}
                            .setWidth(static_cast<uint32_t>(w))
                            .setHeight(static_cast<uint32_t>(h));

  vk::SurfaceFormatKHR surfaceFormat{};
  surfaceFormat.format = vk::Format::eB8G8R8A8Srgb;
  surfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

  vkb::SwapchainBuilder builder =
      vkb::SwapchainBuilder{vkbDevice}
          .set_old_swapchain(swapchain)
          .set_desired_extent(extent.width, extent.height)
          .set_desired_format(surfaceFormat)
          .set_required_min_image_count(capabilities.minImageCount)
          .set_desired_present_mode(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR);

  vkb::Result<vkb::Swapchain> swapchainResult = builder.build();

  VKB_ASSERT(swapchainResult);

  vkbSwapchain = swapchainResult.value();
  swapchain = vkbSwapchain.swapchain;

  vkb::Result<std::vector<VkImageView>> imageViewsResult =
      vkbSwapchain.get_image_views();
  vkb::Result<std::vector<VkImage>> imagesResult = vkbSwapchain.get_images();

  VKB_ASSERT(imageViewsResult);
  VKB_ASSERT(imagesResult);

  swapchainImageViews = imageViewsResult.value();
  swapchainImages = imagesResult.value();
}

void GPU::createAllocator() {
  VmaVulkanFunctions vulkanFunctions{};
  vulkanFunctions.vkGetDeviceProcAddr = instance.fp_vkGetDeviceProcAddr;
  vulkanFunctions.vkGetInstanceProcAddr = instance.fp_vkGetInstanceProcAddr;

  VmaAllocatorCreateInfo createInfo{};
  createInfo.pVulkanFunctions = &vulkanFunctions;
  createInfo.instance = instance.instance;
  createInfo.physicalDevice = physicalDevice.physical_device;
  createInfo.device = device;
  createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  assert(!vmaCreateAllocator(&createInfo, &allocator));
};

void GPU::createQueue() {
  vkb::Result<uint32_t> queueIndexResult =
      vkbDevice.get_queue_index(vkb::QueueType::graphics);

  VKB_ASSERT(queueIndexResult);

  vkb::Result<VkQueue> queueResult =
      vkbDevice.get_queue(vkb::QueueType::graphics);

  VKB_ASSERT(queueResult);

  queue = queueResult.value();
  queueIndex = queueIndexResult.value();
};

void GPU::createSyncPrimitives() {
  fence = device.createFence(
      vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
  renderCompleteSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
  presentCompleteSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
};

void GPU::createCommandPool() {
  vk::CommandPoolCreateInfo commandPoolCreateInfo =
      vk::CommandPoolCreateInfo{}
          .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
          .setQueueFamilyIndex(queueIndex);

  commandPool = device.createCommandPool(commandPoolCreateInfo, nullptr);
};

void GPU::createCommandBuffer() {
  vk::CommandBufferAllocateInfo commandBufferAllocateInfo =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount(1)
          .setLevel(vk::CommandBufferLevel::ePrimary);

  assert(device.allocateCommandBuffers(&commandBufferAllocateInfo,
                                       &commandBuffer) == vk::Result::eSuccess);
};

vk::Sampler GPU::createSampler(const vk::SamplerCreateInfo& createInfo) const {
  return device.createSampler(createInfo);
}

void GPU::destroySampler(const vk::Sampler& sampler) const {
  return device.destroySampler(sampler);
}

void GPU::createDescriptorSetLayouts() {
}

void GPU::createDescriptorSets() {
}

Buffer GPU::createBuffer(const void* data,
                         const vk::DeviceSize size,
                         vk::Flags<vk::BufferUsageFlagBits> usage) const {
  Buffer buffer{};
  buffer.size = size;

  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  bufferAllocationCreateInfo.flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo =
      vk::BufferCreateInfo{}
          .setSize(buffer.size)
          .setUsage(usage)
          .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer b;

  assert(!vmaCreateBuffer(allocator, &bufferCreateInfo,
                          &bufferAllocationCreateInfo, &b, &buffer.allocation,
                          &buffer.allocationInfo));

  buffer.buffer = b;
  assert(!vmaCopyMemoryToAllocation(allocator, data, buffer.allocation, 0, size));

  return buffer;
}

Buffer GPU::createBuffer(const vk::DeviceSize size,
                         const vk::Flags<vk::BufferUsageFlagBits> usage) const {
  Buffer buffer{};
  buffer.size = size;

  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  bufferAllocationCreateInfo.flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo =
      vk::BufferCreateInfo{}
          .setSize(buffer.size)
          .setUsage(usage)
          .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer b;

  assert(!vmaCreateBuffer(allocator, &bufferCreateInfo,
                          &bufferAllocationCreateInfo, &b, &buffer.allocation,
                          &buffer.allocationInfo));

  buffer.buffer = b;
  return buffer;
}

Buffer GPU::createBuffer(const vk::DeviceSize size,
                         const vk::Flags<vk::BufferUsageFlagBits> usage,
                         VmaMemoryUsage memoryUsage,
                         VmaAllocationCreateFlags createFlags) const {
  Buffer buffer{};
  buffer.size = size;

  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = memoryUsage;
  bufferAllocationCreateInfo.flags = createFlags;

  VkBufferCreateInfo bufferCreateInfo =
      vk::BufferCreateInfo{}
          .setSize(buffer.size)
          .setUsage(usage)
          .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer b;

  assert(!vmaCreateBuffer(allocator, &bufferCreateInfo,
                          &bufferAllocationCreateInfo, &b, &buffer.allocation,
                          &buffer.allocationInfo));

  buffer.buffer = b;
  return buffer;
}

void GPU::copyBufferToImage(const Buffer& buffer,
                            const Image& image,
                            const vk::Extent2D& extent,
                            const uint32_t layers) const {
  std::vector<vk::BufferImageCopy2> copyRegions(layers);

  uint32_t offset = 0;

  for (uint32_t i = 0; i < layers; i++) {
    vk::ImageSubresourceLayers imageSubresourceLayers =
        vk::ImageSubresourceLayers{}
            .setLayerCount(1)
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseArrayLayer(i)
            .setMipLevel(0);

    vk::BufferImageCopy2 copyRegion =
        vk::BufferImageCopy2{}
            .setImageOffset(0)
            .setBufferOffset(offset)
            .setBufferRowLength(0)
            .setBufferImageHeight(0)
            .setImageExtent(vk::Extent3D{extent}.setDepth(1))
            .setImageSubresource(imageSubresourceLayers);

    copyRegions[i] = copyRegion;
    offset += (static_cast<uint32_t>(buffer.size) / 6);
  }

  vk::CopyBufferToImageInfo2 copyInfo =
      vk::CopyBufferToImageInfo2{}
          .setSrcBuffer(buffer.buffer)
          .setDstImage(image.image)
          .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
          .setRegionCount(static_cast<uint32_t>(copyRegions.size()))
          .setRegions(copyRegions);

  vk::CommandBuffer copyBufferToImageCmdBuffer = beginSingleSubmitCommand();
  copyBufferToImageCmdBuffer.copyBufferToImage2(copyInfo);
  endSingleSubmitCommand(copyBufferToImageCmdBuffer);
}

vk::DeviceAddress GPU::getBufferDeviceAddress(const Buffer& buffer) const {
  vk::BufferDeviceAddressInfo bufferDeviceAddressInfo =
      vk::BufferDeviceAddressInfo{}.setBuffer(buffer.buffer);

  return device.getBufferAddress(bufferDeviceAddressInfo);
}

vk::CommandBuffer GPU::beginSingleSubmitCommand() const {
  vk::CommandBuffer singleSubmitBuffer;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount(1)
          .setLevel(vk::CommandBufferLevel::ePrimary);

  assert(device.allocateCommandBuffers(&commandBufferAllocateInfo,
                                       &singleSubmitBuffer) ==
         vk::Result::eSuccess);

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}.setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  singleSubmitBuffer.begin(beginInfo);

  return singleSubmitBuffer;
}

void GPU::endSingleSubmitCommand(
    const vk::CommandBuffer& singleSubmitBuffer) const {
  singleSubmitBuffer.end();

  vk::SubmitInfo submitInfo = vk::SubmitInfo{}
                                  .setCommandBuffers(singleSubmitBuffer)
                                  .setCommandBufferCount(1);

  assert(queue.submit(1, &submitInfo, nullptr) == vk::Result::eSuccess);

  queue.waitIdle();
  device.freeCommandBuffers(commandPool, 1, &singleSubmitBuffer);
}

Image GPU::createDepthImage(const DepthImageOptions& options) const {
  Image image{};
  image.extent = vk::Extent3D{options.extent}.setDepth(1);

  VkImageCreateInfo imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(vk::Format::eD32Sfloat)
          .setMipLevels(1)
          .setArrayLayers(1)
          .setSamples(options.samples)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment |
                    options.usage)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setExtent(image.extent);

  VmaAllocationCreateInfo imageAllocationCreateInfo{};
  imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  imageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  VkImage vkImage;

  assert(!vmaCreateImage(allocator, &imageCreateInfo,
                         &imageAllocationCreateInfo, &vkImage,
                         &image.allocation, nullptr));

  image.image = vkImage;

  vk::ImageSubresourceRange imageSubresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eDepth)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo =
      vk::ImageViewCreateInfo{}
          .setImage(image.image)
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(vk::Format::eD32Sfloat)
          .setSubresourceRange(imageSubresourceRange);

  image.view = device.createImageView(imageViewCreateInfo, nullptr);

  return image;
}

Image GPU::createTexture2D(const uint8_t* data,
                           const vk::Extent2D& extent,
                           const vk::Format format) {
  Image image{};
  image.extent = vk::Extent3D{extent}.setDepth(1);
  image.mipLevels = utils::getMipLevels(extent.height, extent.width);

  VkImageCreateInfo imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(format)
          .setMipLevels(image.mipLevels)
          .setArrayLayers(1)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                    vk::ImageUsageFlagBits::eTransferDst |
                    vk::ImageUsageFlagBits::eSampled)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setExtent(image.extent);

  VmaAllocationCreateInfo imageAllocationCreateInfo{};
  imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  imageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  VkImage vkImage;

  assert(!vmaCreateImage(allocator, &imageCreateInfo,
                         &imageAllocationCreateInfo, &vkImage,
                         &image.allocation, nullptr));

  image.image = vkImage;

  vk::ImageSubresourceRange imageSubresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseMipLevel(0)
          .setLevelCount(image.mipLevels)
          .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo =
      vk::ImageViewCreateInfo{}
          .setImage(image.image)
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(format)
          .setSubresourceRange(imageSubresourceRange);

  uint32_t size = extent.width * extent.height * STBI_rgb_alpha;

  ImageMemoryBarrierOptions options;
  options.newLayout = vk::ImageLayout::eTransferDstOptimal;
  options.image = image.image;

  vk::CommandBuffer transitionCmd = beginSingleSubmitCommand();
  addImageMemoryBarrier(transitionCmd, options);
  endSingleSubmitCommand(transitionCmd);

  Buffer stagingBuffer =
      createBuffer(data, size, vk::BufferUsageFlagBits::eTransferSrc);

  copyBufferToImage(stagingBuffer, image, extent);

  generateMipmaps(image);

  options.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
  options.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  options.levelCount = image.mipLevels;

  transitionCmd = beginSingleSubmitCommand();
  addImageMemoryBarrier(transitionCmd, options);
  endSingleSubmitCommand(transitionCmd);

  destroyBuffer(stagingBuffer);

  image.view = device.createImageView(imageViewCreateInfo, nullptr);

  return image;
}

Image GPU::createCubemapTexture(const std::array<uint8_t*, 6>& data,
                                const vk::Extent2D& extent) {
  Image image{};
  image.extent = vk::Extent3D{extent}.setDepth(1);

  VkImageCreateInfo imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(vk::Format::eR8G8B8A8Srgb)
          .setFlags(vk::ImageCreateFlagBits::eCubeCompatible)
          .setMipLevels(1)
          .setArrayLayers(6)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eTransferDst |
                    vk::ImageUsageFlagBits::eSampled)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setExtent(image.extent);

  VmaAllocationCreateInfo imageAllocationCreateInfo{};
  imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  imageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  VkImage vkImage;

  assert(!vmaCreateImage(allocator, &imageCreateInfo,
                         &imageAllocationCreateInfo, &vkImage,
                         &image.allocation, nullptr));

  image.image = vkImage;

  vk::ImageSubresourceRange imageSubresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(6)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo =
      vk::ImageViewCreateInfo{}
          .setImage(image.image)
          .setViewType(vk::ImageViewType::eCube)
          .setFormat(vk::Format::eR8G8B8A8Srgb)
          .setSubresourceRange(imageSubresourceRange);

  image.view = device.createImageView(imageViewCreateInfo, nullptr);

  uint32_t size = extent.width * extent.height * STBI_rgb_alpha;
  uint32_t totalSize = size * 6;

  ImageMemoryBarrierOptions options;
  options.image = image.image;
  options.layers = 6;
  options.newLayout = vk::ImageLayout::eTransferDstOptimal;

  vk::CommandBuffer transitionCmd = beginSingleSubmitCommand();
  addImageMemoryBarrier(transitionCmd, options);
  endSingleSubmitCommand(transitionCmd);

  Buffer stagingBuffer =
      createBuffer(totalSize, vk::BufferUsageFlagBits::eTransferSrc);

  uint32_t offset = 0;
  for (const uint8_t* ptr : data) {
    assert(!vmaCopyMemoryToAllocation(allocator, ptr, stagingBuffer.allocation,
                                      offset, size));
    offset += size;
  }

  copyBufferToImage(stagingBuffer, image, extent, 6);

  options.oldLayout = options.newLayout;
  options.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

  transitionCmd = beginSingleSubmitCommand();
  addImageMemoryBarrier(transitionCmd, options);
  endSingleSubmitCommand(transitionCmd);

  destroyBuffer(stagingBuffer);

  return image;
}

void GPU::generateMipmaps(const Image& image) {
  vk::CommandBuffer cmdBuffer = beginSingleSubmitCommand();

  ImageMemoryBarrierOptions options;
  options.image = image.image;
  options.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  options.newLayout = vk::ImageLayout::eTransferSrcOptimal;

  addImageMemoryBarrier(cmdBuffer, options);
  endSingleSubmitCommand(cmdBuffer);

  for (uint32_t i = 1; i < image.mipLevels; i++) {
    cmdBuffer = beginSingleSubmitCommand();

    vk::ImageSubresourceLayers src =
        vk::ImageSubresourceLayers{}
            .setMipLevel(i - 1)
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setLayerCount(1)
            .setBaseArrayLayer(0);

    vk::ImageSubresourceLayers dst =
        vk::ImageSubresourceLayers{}
            .setMipLevel(i)
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setLayerCount(1)
            .setBaseArrayLayer(0);

    std::array<vk::Offset3D, 2> srcOffsets{};
    srcOffsets[1].x = static_cast<int32_t>(image.extent.width >> (i - 1));
    srcOffsets[1].y = static_cast<int32_t>(image.extent.height >> (i - 1));
    srcOffsets[1].z = 1;

    std::array<vk::Offset3D, 2> dstOffsets{};
    dstOffsets[1].x = static_cast<int32_t>(image.extent.width >> i);
    dstOffsets[1].y = static_cast<int32_t>(image.extent.height >> i);
    dstOffsets[1].z = 1;

    vk::ImageBlit imageBlit = vk::ImageBlit{}
                                  .setSrcSubresource(src)
                                  .setDstSubresource(dst)
                                  .setSrcOffsets(srcOffsets)
                                  .setDstOffsets(dstOffsets);

    options.image = image.image;
    options.mipLevel = i;
    options.oldLayout = vk::ImageLayout::eUndefined;
    options.newLayout = vk::ImageLayout::eTransferDstOptimal;
    options.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    options.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    options.srcAccessMask = vk::AccessFlagBits2::eNone;
    options.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;

    addImageMemoryBarrier(cmdBuffer, options);

    cmdBuffer.blitImage(image.image, vk::ImageLayout::eTransferSrcOptimal,
                        image.image, vk::ImageLayout::eTransferDstOptimal, 1,
                        &imageBlit, vk::Filter::eLinear);

    options.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    options.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    options.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    options.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    options.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    options.dstAccessMask = vk::AccessFlagBits2::eTransferRead;

    addImageMemoryBarrier(cmdBuffer, options);

    endSingleSubmitCommand(cmdBuffer);
  }
}

void GPU::addImageMemoryBarrier(vk::CommandBuffer& cmdBuffer,
                                const ImageMemoryBarrierOptions& options) {
  vk::ImageSubresourceRange subresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(options.layers)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseMipLevel(options.mipLevel)
          .setLevelCount(options.levelCount)
          .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 imageMemoryBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(options.image)
          .setOldLayout(options.oldLayout)
          .setNewLayout(options.newLayout)
          .setSrcAccessMask(options.srcAccessMask)
          .setDstAccessMask(options.dstAccessMask)
          .setSrcStageMask(options.srcStageMask)
          .setDstStageMask(options.dstStageMask)
          .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarriers[1] = {imageMemoryBarrier};

  vk::DependencyInfo dependencyInfo =
      vk::DependencyInfo{}
          .setImageMemoryBarriers(imageMemoryBarriers)
          .setImageMemoryBarrierCount(1);

  cmdBuffer.pipelineBarrier2(dependencyInfo);
}

void GPU::destroyBuffer(const Buffer& buffer) const {
  vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void GPU::destroyImage(const Image& image) const {
  device.destroyImageView(image.view);
  vmaDestroyImage(allocator, image.image, image.allocation);
}

void GPU::createViewportAndScissors() {
  vk::Extent2D extent = vk::Extent2D{vkbSwapchain.extent};

  viewport = vk::Viewport{}
                 .setWidth(static_cast<float>(extent.width))
                 .setHeight(static_cast<float>(extent.height))
                 .setMaxDepth(1.0)
                 .setMinDepth(0.0)
                 .setX(0.0)
                 .setY(0.0);

  scissors = vk::Rect2D{}.setExtent(
      vk::Extent2D{}.setHeight(extent.height).setWidth(extent.width));
};

void GPU::rebuiltSwapchain() {
  device.waitIdle();

  destroySwapchainResources();

  vkb::Swapchain old = vkbSwapchain;

  createSwapchain();
  createImages();
  createViewportAndScissors();

  vkb::destroy_swapchain(old);
}

void GPU::destroySwapchainResources() {
  for (const vk::ImageView imageView : swapchainImageViews) {
    device.destroyImageView(imageView);
  }

  device.destroyImageView(depthImage.view);
  vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);

  device.destroyImageView(multisampleImage.view);
  vmaDestroyImage(allocator, multisampleImage.image,
                  multisampleImage.allocation);
}

Shader GPU::loadShader(const std::string_view path,
                       vk::ShaderStageFlagBits stage) const {
  Shader shader{};
  shader.stage = stage;

  std::vector<char> file = utils::readFile(path);

  vk::ShaderModuleCreateInfo createInfo =
      vk::ShaderModuleCreateInfo{}
          .setPCode(reinterpret_cast<uint32_t*>(file.data()))
          .setCodeSize(file.size());

  shader.module = device.createShaderModule(createInfo, VK_NULL_HANDLE);

  return shader;
}

void GPU::destroyShader(const Shader& shader) const {
  device.destroyShaderModule(shader.module);
}

Pipeline GPU::createPipeline(const PipelineOptions& options) const {
  assert(options.pushConstantSize <=
         physicalDeviceProperties.properties.limits.maxPushConstantsSize);

  Pipeline pipeline{};

  std::vector<vk::VertexInputBindingDescription> inputBindings{
      vk::VertexInputBindingDescription{}
          .setStride(sizeof(Vertex))
          .setInputRate(vk::VertexInputRate::eVertex)
          .setBinding(0)};

  vk::VertexInputAttributeDescription vertexPositionAttributeDescription =
      vk::VertexInputAttributeDescription{}
          .setBinding(0)
          .setLocation(0)
          .setOffset(offsetof(Vertex, position))
          .setFormat(vk::Format::eR32G32B32Sfloat);

  vk::VertexInputAttributeDescription vertexColorAttributeDescription =
      vk::VertexInputAttributeDescription{}
          .setBinding(0)
          .setLocation(1)
          .setOffset(offsetof(Vertex, clr))
          .setFormat(vk::Format::eR32G32B32A32Sfloat);

  vk::VertexInputAttributeDescription vertexTextureCoordAttributeDescription =
      vk::VertexInputAttributeDescription{}
          .setBinding(0)
          .setLocation(2)
          .setOffset(offsetof(Vertex, uv))
          .setFormat(vk::Format::eR32G32Sfloat);

  vk::VertexInputAttributeDescription vertexNormalsAttributeDescription =
      vk::VertexInputAttributeDescription{}
          .setBinding(0)
          .setLocation(3)
          .setOffset(offsetof(Vertex, normal))
          .setFormat(vk::Format::eR32G32B32Sfloat);

  vk::VertexInputAttributeDescription vertexTangentAttributeDescription =
      vk::VertexInputAttributeDescription{}
          .setBinding(0)
          .setLocation(4)
          .setOffset(offsetof(Vertex, tangent))
          .setFormat(vk::Format::eR32G32B32A32Sfloat);

  std::vector<vk::VertexInputAttributeDescription> inputAttributes = {
      vertexPositionAttributeDescription, vertexColorAttributeDescription,
      vertexTextureCoordAttributeDescription, vertexNormalsAttributeDescription,
      vertexTangentAttributeDescription};

  std::vector<vk::PipelineShaderStageCreateInfo> stages{};
  vk::Flags<vk::ShaderStageFlagBits> stageFlagBits = options.shaders[0].stage;

  for (const Shader& shader : options.shaders) {
    stages.push_back(
      vk::PipelineShaderStageCreateInfo{}
            .setStage(shader.stage)
            .setModule(shader.module)
            .setPName("main")
            .setPSpecializationInfo(nullptr)
    );

    stageFlagBits = stageFlagBits | shader.stage;
  }

  vk::Format colorAttachmentFormat = vk::Format::eB8G8R8A8Srgb;
  vk::Format depthAttachmentFormat = vk::Format::eD32Sfloat;

  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo =
      vk::PipelineRenderingCreateInfo{}
          .setDepthAttachmentFormat(depthAttachmentFormat)
          .setColorAttachmentCount(0);

  if (options.colorAttachmentCount) {
    pipelineRenderingCreateInfo 
      .setColorAttachmentCount(options.colorAttachmentCount)
      .setColorAttachmentFormats(colorAttachmentFormat);
  }

  vk::PushConstantRange pushConstantRange =
      vk::PushConstantRange{}
          .setOffset(0)
          .setSize(options.pushConstantSize)
          .setStageFlags(stageFlagBits);

  vk::PipelineVertexInputStateCreateInfo vertexInputState =
      vk::PipelineVertexInputStateCreateInfo{}
          .setVertexBindingDescriptions(inputBindings)
          .setVertexBindingDescriptionCount(inputBindings.size())
          .setVertexAttributeDescriptionCount(inputAttributes.size())
          .setVertexAttributeDescriptions(inputAttributes);

  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
      vk::PipelineInputAssemblyStateCreateInfo{}
          .setTopology(vk::PrimitiveTopology::eTriangleList)
          .setPrimitiveRestartEnable(0);

  vk::PipelineViewportStateCreateInfo viewportState =
      vk::PipelineViewportStateCreateInfo{}
          .setScissors(scissors)
          .setViewports(viewport)
          .setViewportCount(1)
          .setScissorCount(1);

  vk::PipelineRasterizationStateCreateInfo rasterizationState =
      vk::PipelineRasterizationStateCreateInfo{}
          .setRasterizerDiscardEnable(0)
          .setDepthClampEnable(0)
          .setDepthBiasEnable(options.depthConstantBias || options.depthSlopeBias)
          .setDepthBiasConstantFactor(options.depthConstantBias)
          .setDepthBiasSlopeFactor(options.depthSlopeBias)
          .setPolygonMode(vk::PolygonMode::eFill)
          .setFrontFace(vk::FrontFace::eCounterClockwise)
          .setLineWidth(1.0f);

  vk::PipelineMultisampleStateCreateInfo multisampleState =
      vk::PipelineMultisampleStateCreateInfo{}
          .setRasterizationSamples(sampleCount)
          .setMinSampleShading(0.2)
          .setSampleShadingEnable(1);

  vk::PipelineDepthStencilStateCreateInfo depthStencilState =
      vk::PipelineDepthStencilStateCreateInfo{}
          .setDepthTestEnable(1)
          .setStencilTestEnable(0)
          .setDepthBoundsTestEnable(0)
          .setDepthCompareOp(vk::CompareOp::eLessOrEqual);

  vk::PipelineColorBlendAttachmentState colorBlendAttachmentState =
      vk::PipelineColorBlendAttachmentState{}
          .setColorWriteMask(
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
          .setSrcColorBlendFactor(vk::BlendFactor::eOne)
          .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
          .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
          .setDstAlphaBlendFactor(vk::BlendFactor::eZero);

  vk::PipelineColorBlendStateCreateInfo colorBlendState =
      vk::PipelineColorBlendStateCreateInfo{}
          .setAttachmentCount(1)
          .setAttachments(colorBlendAttachmentState);

  std::vector<vk::DynamicState> dynamicStates{
      vk::DynamicState::eViewport, vk::DynamicState::eScissor,
      vk::DynamicState::eCullMode, vk::DynamicState::eColorBlendEnableEXT,
      vk::DynamicState::eDepthWriteEnable};

  vk::PipelineDynamicStateCreateInfo dynamicState =
      vk::PipelineDynamicStateCreateInfo{}
          .setDynamicStates(dynamicStates)
          .setDynamicStateCount(dynamicStates.size());

  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{};

  for (const vk::DescriptorSetLayoutCreateInfo& createInfo : options.descriptorSetLayoutCreateInfos) {
    descriptorSetLayouts.push_back(device.createDescriptorSetLayout(createInfo));
  }

  pipeline.descriptorSetLayouts = descriptorSetLayouts;

  vk::DescriptorSetAllocateInfo allocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorSetCount(options.descriptorSetLayoutCreateInfos.size())
    .setDescriptorPool(descriptorPool)
    .setSetLayouts(descriptorSetLayouts);

  pipeline.descriptorSets = device.allocateDescriptorSets(allocateInfo);

  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo =
      vk::PipelineLayoutCreateInfo{}
          .setPushConstantRanges(pushConstantRange)
          .setPushConstantRangeCount(1)
          .setSetLayouts(descriptorSetLayouts)
          .setSetLayoutCount(descriptorSetLayouts.size());

  pipeline.layout =
      device.createPipelineLayout(pipelineLayoutCreateInfo, nullptr);

  vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo =
      vk::GraphicsPipelineCreateInfo{}
          .setPNext(&pipelineRenderingCreateInfo)
          .setStages(stages)
          .setStageCount(stages.size())
          .setPVertexInputState(&vertexInputState)
          .setPInputAssemblyState(&inputAssemblyState)
          .setPTessellationState(VK_NULL_HANDLE)
          .setPViewportState(&viewportState)
          .setPRasterizationState(&rasterizationState)
          .setPMultisampleState(&multisampleState)
          .setPDepthStencilState(&depthStencilState)
          .setPColorBlendState(&colorBlendState)
          .setPDynamicState(&dynamicState)
          .setRenderPass(VK_NULL_HANDLE)
          .setLayout(pipeline.layout);

  vk::ResultValue<vk::Pipeline> pipelineResult =
      device.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);

  assert(pipelineResult.result == vk::Result::eSuccess);

  pipeline.shaders = options.shaders;
  pipeline.pipeline = pipelineResult.value;

  return pipeline;
}

void GPU::destroyPipeline(const Pipeline& pipeline) const {
  for (const Shader& shader : pipeline.shaders) {
    destroyShader(shader);
  }
  for (const vk::DescriptorSetLayout& setLayout : pipeline.descriptorSetLayouts) {
    device.destroyDescriptorSetLayout(setLayout);
  }
  device.destroyPipelineLayout(pipeline.layout);
  device.destroyPipeline(pipeline.pipeline);
}

void GPU::waitForFence() const {
  assert(device.waitForFences(1, &fence, 1, UINT64_MAX) ==
         vk::Result::eSuccess);
}

int32_t GPU::acquireNextImage() const {
  uint32_t imageIndex;
  vk::Result acquireResult = device.acquireNextImageKHR(
      swapchain, UINT64_MAX, presentCompleteSemaphore, nullptr, &imageIndex);

  switch (acquireResult) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eSuboptimalKHR:
      return -1;
    case vk::Result::eErrorOutOfDateKHR:
      return -1;
    case vk::Result::eNotReady:
    default:
      DEBUG_BREAK();
      break;
  }

  return static_cast<int32_t>(imageIndex);
}

void GPU::resetFence() const {
  assert(device.resetFences(1, &fence) == vk::Result::eSuccess);
}

void GPU::beginShadowPass(const Texture& shadowMap) const {
  vk::ClearValue depthClearValue = vk::ClearValue{}.setDepthStencil(
      vk::ClearDepthStencilValue{}.setDepth(1.0f).setStencil(0));

  vk::RenderingAttachmentInfo depthAttachment =
      vk::RenderingAttachmentInfo{}
          .setImageView(shadowMap.image.view)
          .setResolveMode(vk::ResolveModeFlagBits::eNone)
          .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setClearValue(depthClearValue);

  vk::ImageSubresourceRange depthSubresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eDepth)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 depthMemoryBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(shadowMap.image.image)
          .setOldLayout(vk::ImageLayout::eUndefined)
          .setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
          .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
          .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                            vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits2::eLateFragmentTests)
          .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits2::eLateFragmentTests)
          .setSubresourceRange(depthSubresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarriers[1] = {depthMemoryBarrier};

  vk::DependencyInfo dependencyInfo =
      vk::DependencyInfo{}
          .setImageMemoryBarriers(imageMemoryBarriers)
          .setImageMemoryBarrierCount(1);

  vk::RenderingInfo renderingInfo =
      vk::RenderingInfo{}
          .setColorAttachmentCount(0)
          .setRenderArea(vk::Rect2D{}.setExtent(
              vk::Extent2D{shadowSize, shadowSize}))
          .setLayerCount(1)
          .setViewMask(0)
          .setPDepthAttachment(&depthAttachment);

  commandBuffer.pipelineBarrier2(dependencyInfo);
  commandBuffer.beginRendering(renderingInfo);
}

void GPU::beginMainPass(const uint32_t imageIndex) {
  vk::ImageView swapImageView = swapchainImageViews[imageIndex];
  vk::Image swapImage = swapchainImages[imageIndex];

  vk::ClearValue clearValue = vk::ClearValue{}.setColor(
      vk::ClearColorValue{}.setFloat32({0.0, 0.0, 0.0, 0.0}));

  vk::ClearValue depthClearValue = vk::ClearValue{}.setDepthStencil(
      vk::ClearDepthStencilValue{}.setDepth(1.0f).setStencil(0));

  vk::RenderingAttachmentInfo depthAttachment =
      vk::RenderingAttachmentInfo{}
          .setImageView(depthImage.view)
          .setResolveMode(vk::ResolveModeFlagBits::eNone)
          .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eNone)
          .setClearValue(depthClearValue);

  vk::RenderingAttachmentInfo colorAttachment =
      vk::RenderingAttachmentInfo{}
          .setImageView(multisampleImage.view)
          .setResolveImageView(swapImageView)
          .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setResolveMode(vk::ResolveModeFlagBits::eAverage)
          .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setClearValue(clearValue);

  vk::RenderingInfo renderingInfo = vk::RenderingInfo{}
                                        .setRenderArea(scissors)
                                        .setLayerCount(1)
                                        .setViewMask(0)
                                        .setColorAttachmentCount(1)
                                        .setColorAttachments(colorAttachment)
                                        .setPDepthAttachment(&depthAttachment);

  vk::ImageSubresourceRange depthSubresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eDepth)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageSubresourceRange subresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 depthMemoryBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(depthImage.image)
          .setOldLayout(vk::ImageLayout::eUndefined)
          .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
          .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
          .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                            vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits2::eLateFragmentTests)
          .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                           vk::PipelineStageFlagBits2::eLateFragmentTests)
          .setSubresourceRange(depthSubresourceRange);

  vk::ImageMemoryBarrier2 multisampleImageBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(multisampleImage.image)
          .setOldLayout(vk::ImageLayout::eUndefined)
          .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setSrcAccessMask(vk::AccessFlagBits2::eNone)
          .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
          .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
          .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(swapImage)
          .setOldLayout(vk::ImageLayout::eUndefined)
          .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setSrcAccessMask(vk::AccessFlagBits2::eNone)
          .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
          .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
          .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 presentImageMemoryBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(swapImage)
          .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
          .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
          .setDstAccessMask(vk::AccessFlagBits2::eNone)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
          .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
          .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarriers[4] = {
      depthMemoryBarrier, imageMemoryBarrier, multisampleImageBarrier,
      presentImageMemoryBarrier};

  vk::DependencyInfo dependencyInfo =
      vk::DependencyInfo{}
          .setImageMemoryBarriers(imageMemoryBarriers)
          .setImageMemoryBarrierCount(4);

  commandBuffer.pipelineBarrier2(dependencyInfo);
  commandBuffer.beginRendering(renderingInfo);
}

void GPU::createMultiSampleImage() {
  Image image{};
  image.extent = vk::Extent3D{vkbSwapchain.extent}.setDepth(1);

  VkImageCreateInfo imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(vk::Format::eB8G8R8A8Srgb)
          .setMipLevels(1)
          .setArrayLayers(1)
          .setSamples(sampleCount)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eColorAttachment)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setExtent(image.extent);

  VmaAllocationCreateInfo imageAllocationCreateInfo{};
  imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  imageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  VkImage vkImage;

  vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo,
                 &vkImage, &image.allocation, nullptr);

  image.image = vkImage;

  vk::ImageSubresourceRange imageSubresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo =
      vk::ImageViewCreateInfo{}
          .setImage(image.image)
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(vk::Format::eB8G8R8A8Srgb)
          .setSubresourceRange(imageSubresourceRange);

  image.view = device.createImageView(imageViewCreateInfo, nullptr);
  multisampleImage = image;
}

void GPU::createImages() {
  depthImage = createDepthImage(
      {sampleCount, vk::ImageUsageFlagBits::eDepthStencilAttachment,
       vkbSwapchain.extent});
  createMultiSampleImage();
}

void GPU::submit(const uint32_t imageIndex) {
  vk::Flags<vk::PipelineStageFlagBits> waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;

  vk::SubmitInfo submitInfo = vk::SubmitInfo{}
                                  .setWaitSemaphoreCount(1)
                                  .setWaitSemaphores(presentCompleteSemaphore)
                                  .setCommandBuffers(commandBuffer)
                                  .setCommandBufferCount(1)
                                  .setSignalSemaphores(renderCompleteSemaphore)
                                  .setSignalSemaphoreCount(1)
                                  .setWaitDstStageMask(waitStage);

  vk::Result queueSubmitResult = queue.submit(1, &submitInfo, fence);

  assert(queueSubmitResult == vk::Result::eSuccess);

  uint32_t imageIndices = {static_cast<uint32_t>(imageIndex)};

  vk::PresentInfoKHR presentInfo =
      vk::PresentInfoKHR{}
          .setWaitSemaphoreCount(1)
          .setWaitSemaphores(renderCompleteSemaphore)
          .setSwapchainCount(1)
          .setSwapchains(swapchain)
          .setImageIndices(imageIndices);

  vk::Result presentResult = queue.presentKHR(&presentInfo);

  switch (presentResult) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eSuboptimalKHR:
      rebuiltSwapchain();
      break;
    case vk::Result::eErrorOutOfDateKHR:
      rebuiltSwapchain();
      break;
    case vk::Result::eNotReady:
      DEBUG_BREAK();
      break;
    default:
      DEBUG_BREAK();
      break;
  }
}

void GPU::createPipelines() {
  std::vector<vk::DescriptorPoolSize> poolSizes = {
    vk::DescriptorPoolSize{}
        .setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(10),
    vk::DescriptorPoolSize{}
        .setType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(300),
    vk::DescriptorPoolSize{}
        .setType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(10),
  };

  uint32_t sets = 0;

  std::vector<std::vector<vk::DescriptorSetLayoutBinding>> mainPassDescriptorSetLayoutBindings{
    {
      vk::DescriptorSetLayoutBinding{}
        .setBinding(0)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eAll)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
    },
    {
      vk::DescriptorSetLayoutBinding{}
        .setBinding(0)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer),
      vk::DescriptorSetLayoutBinding{}
        .setBinding(1)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
    },
    {
      vk::DescriptorSetLayoutBinding{}
        .setBinding(0)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(100),
      vk::DescriptorSetLayoutBinding{}
        .setBinding(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(100),
      vk::DescriptorSetLayoutBinding{}
        .setBinding(2)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(100),
    },
    {
      vk::DescriptorSetLayoutBinding{}
        .setBinding(0)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer),
      vk::DescriptorSetLayoutBinding{}
        .setBinding(1)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer),
      vk::DescriptorSetLayoutBinding{}
        .setBinding(2)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer),
      vk::DescriptorSetLayoutBinding{}
        .setBinding(3)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
    },
  };

  std::vector<vk::DescriptorSetLayoutCreateInfo> mainPassDescriptorSets = utils::getDescriptorSetLayoutCreateInfo(mainPassDescriptorSetLayoutBindings);
  sets += mainPassDescriptorSets.size();

  std::vector<std::vector<vk::DescriptorSetLayoutBinding>> skyboxPassDescriptorSetLayoutBindings{
    {
      vk::DescriptorSetLayoutBinding{}
        .setBinding(0)
	    .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
    },
  };

  std::vector<vk::DescriptorSetLayoutCreateInfo> skyboxDescriptorSets = utils::getDescriptorSetLayoutCreateInfo(skyboxPassDescriptorSetLayoutBindings);
  sets += skyboxDescriptorSets.size();

  std::vector<std::vector<vk::DescriptorSetLayoutBinding>> shadowPassDescriptorSetLayoutBindings{
    {
      vk::DescriptorSetLayoutBinding{}
        .setBinding(0)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
    },
  };

  std::vector<vk::DescriptorSetLayoutCreateInfo> shadowDescriptorSets = utils::getDescriptorSetLayoutCreateInfo(shadowPassDescriptorSetLayoutBindings);
  sets += shadowDescriptorSets.size();

  vk::DescriptorPoolCreateInfo poolCreateInfo = vk::DescriptorPoolCreateInfo{}
    .setPoolSizes(poolSizes)
    .setPoolSizeCount(poolSizes.size())
    .setMaxSets(sets);

  descriptorPool = device.createDescriptorPool(poolCreateInfo);

  mainPipeline =
      createPipeline({{loadShader("./shaders/shader.vert.glsl.spv",
                                vk::ShaderStageFlagBits::eVertex),
                     loadShader("./shaders/shader.frag.glsl.spv",
                                vk::ShaderStageFlagBits::eFragment)},
                     mainPassDescriptorSets, sizeof(MainPassFrameData), 1});


  skyboxPipeline =
      createPipeline({{loadShader("./shaders/skybox.vert.glsl.spv",
                                vk::ShaderStageFlagBits::eVertex),
                     loadShader("./shaders/skybox.frag.glsl.spv",
                                vk::ShaderStageFlagBits::eFragment)},
                     skyboxDescriptorSets, sizeof(SkyboxPassFrameData), 1});

  shadowsPipeline =
      createPipeline({
      {loadShader("./shaders/shadows.vert.glsl.spv",
                                vk::ShaderStageFlagBits::eVertex)},
        shadowDescriptorSets, 
        sizeof(ShadowPassFrameData),
      0,
      1.25f,
      1.75f
  });
};

void GPU::beginRecordingCommands() const {
  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}.setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);
}

Image GPU::createShadowMap() const {
  return createDepthImage(
      {sampleCount, vk::ImageUsageFlagBits::eSampled, vk::Extent2D{shadowSize, shadowSize}});
}
