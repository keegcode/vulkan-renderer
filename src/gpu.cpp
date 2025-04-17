#include "gpu.hpp"
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "stb_image.h"
#include "utils.hpp"

GPU::GPU(const Display& d) : display{d} {};

void GPU::createInstance() {
  vkb::Result<vkb::Instance> instanceResult =
      vkb::InstanceBuilder{}
          .set_app_name("VkRenderer")
          .require_api_version(1, 3)
          .enable_extensions(display.vulkanExtensions)
          .enable_validation_layers(true)
          .use_default_debug_messenger()
          .build();

  if (!instanceResult) {
    throw std::runtime_error{"Failed to create instance: " +
                             instanceResult.error().message()};
  }

  instance = instanceResult.value();
  dld.init(instance.instance, instance.fp_vkGetInstanceProcAddr);
};

void GPU::destroy() {
  device.destroySampler(diffuseSampler);
  device.destroySampler(specularSampler);
  device.destroySampler(skyboxSampler);

  device.destroyDescriptorSetLayout(uniformLayout);
  device.destroyDescriptorSetLayout(textureLayout);
  device.destroyDescriptorSetLayout(lightLayout);
  device.destroyDescriptorSetLayout(storageBufferLayout);
  device.destroyDescriptorSetLayout(skyboxLayout);

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
      vk::EXTDescriptorBufferExtensionName,
  };

  vkb::Result<vkb::PhysicalDevice> physicalDeviceResult =
      vkb::PhysicalDeviceSelector{instance}
          .set_surface(surface)
          .set_minimum_version(1, 3)
          .require_present(true)
          .add_required_extensions(extensions)
          .add_required_extension_features(
              vk::PhysicalDeviceDynamicRenderingFeatures{}.setDynamicRendering(
                  1))
          .add_required_extension_features(
              vk::PhysicalDeviceSynchronization2Features{}.setSynchronization2(
                  1))
          .add_required_extension_features(
              vk::PhysicalDeviceDescriptorBufferFeaturesEXT{}
                  .setDescriptorBuffer(1))
          .add_required_extension_features(
              vk::PhysicalDeviceBufferDeviceAddressFeatures{}
                  .setBufferDeviceAddress(1))
          .select();

  if (!physicalDeviceResult) {
    throw std::runtime_error{"Failed to select physical device: " +
                             physicalDeviceResult.error().message()};
  }

  physicalDevice = physicalDeviceResult.value();
  physicalDeviceProperties.pNext = &descriptorBufferProperties;

  vk::PhysicalDevice pd = vk::PhysicalDevice{physicalDevice};

  pd.getProperties2(&physicalDeviceProperties);

  capabilities = pd.getSurfaceCapabilitiesKHR(physicalDevice.surface);
};

void GPU::pickDevice() {
  vkb::Result<vkb::Device> deviceResult =
      vkb::DeviceBuilder{physicalDevice}.build();

  if (!deviceResult) {
    throw std::runtime_error{"Failed to create a device: " +
                             deviceResult.error().message()};
  }

  vkbDevice = deviceResult.value();
  device = vk::Device{vkbDevice.device};
};

void GPU::createSwapchain() {
  int32_t w, h;
  SDL_GetWindowSize(display.window, &w, &h);

  vk::Extent2D extent = vk::Extent2D{}.setWidth(w).setHeight(h);

  vkb::SwapchainBuilder builder =
      vkb::SwapchainBuilder{vkbDevice}
          .set_old_swapchain(swapchain)
          .set_desired_extent(extent.width, extent.height)
          .set_required_min_image_count(capabilities.minImageCount)
          .set_desired_present_mode(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR);

  vkb::Result<vkb::Swapchain> swapchainResult = builder.build();

  if (!swapchainResult) {
    throw std::runtime_error{"Failed to create a swapchain: " +
                             swapchainResult.error().message()};
  }

  vkbSwapchain = swapchainResult.value();
  swapchain = vkbSwapchain.swapchain;

  vkb::Result<std::vector<VkImageView>> imageViewsResult =
      vkbSwapchain.get_image_views();
  vkb::Result<std::vector<VkImage>> imagesResult = vkbSwapchain.get_images();

  if (!imageViewsResult) {
    throw std::runtime_error{"Failed to get image views from swapchain"};
  }

  if (!imagesResult) {
    throw std::runtime_error{"Failed to get imags from swapchain"};
  }

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

  if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create a VmaAllocator"};
  };
};

void GPU::createQueue() {
  vkb::Result<uint32_t> queueIndexResult =
      vkbDevice.get_queue_index(vkb::QueueType::graphics);

  if (!queueIndexResult) {
    throw std::runtime_error{"Failed to get a queue index" +
                             queueIndexResult.error().message()};
  }

  vkb::Result<VkQueue> queueResult =
      vkbDevice.get_queue(vkb::QueueType::graphics);

  if (!queueResult) {
    throw std::runtime_error{"Failed to get a queue" +
                             queueResult.error().message()};
  }

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

  if (device.allocateCommandBuffers(&commandBufferAllocateInfo,
                                    &commandBuffer) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };
};

void GPU::createSampler() {
  vk::SamplerCreateInfo samplerCreateInfo =
      vk::SamplerCreateInfo{}
          .setMagFilter(vk::Filter::eLinear)
          .setMinFilter(vk::Filter::eLinear)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(0)
          .setCompareEnable(0);

  diffuseSampler = device.createSampler(samplerCreateInfo);
  specularSampler = device.createSampler(samplerCreateInfo);
  skyboxSampler = device.createSampler(samplerCreateInfo);
}

void GPU::createDescriptorSetLayouts() {
  vk::DescriptorSetLayoutBinding imageSamplerBinding =
      vk::DescriptorSetLayoutBinding{}
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler);

  vk::DescriptorSetLayoutBinding uniformBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutBinding diffuseMapBidning =
      vk::DescriptorSetLayoutBinding{imageSamplerBinding}
          .setBinding(0)
          .setImmutableSamplers(diffuseSampler);

  vk::DescriptorSetLayoutBinding specularMapBinding =
      vk::DescriptorSetLayoutBinding{imageSamplerBinding}
          .setBinding(1)
          .setImmutableSamplers(specularSampler);

  std::vector<vk::DescriptorSetLayoutBinding> textureBindings{
      diffuseMapBidning, specularMapBinding};

  vk::DescriptorSetLayoutCreateInfo textureSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(textureBindings)
          .setBindingCount(textureBindings.size())
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  std::vector<vk::DescriptorSetLayoutBinding> lightBindings = {
      vk::DescriptorSetLayoutBinding{uniformBinding}.setBinding(0),
      vk::DescriptorSetLayoutBinding{uniformBinding}
          .setBinding(1)
          .setDescriptorCount(8),
      vk::DescriptorSetLayoutBinding{uniformBinding}
          .setBinding(2)
          .setDescriptorCount(8),
  };

  vk::DescriptorSetLayoutBinding storageBufferBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eStorageBuffer);

  vk::DescriptorSetLayoutBinding skyboxBinding =
      vk::DescriptorSetLayoutBinding{imageSamplerBinding}
          .setBinding(0)
          .setImmutableSamplers(skyboxSampler);

  std::vector<vk::DescriptorSetLayoutBinding> skyboxBindings{
      skyboxBinding};

  vk::DescriptorSetLayoutCreateInfo lightSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(lightBindings)
          .setBindingCount(lightBindings.size())
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo uniformSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(uniformBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo storageBufferSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(storageBufferBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo skyboxSetLayoutCreateInfo =
    vk::DescriptorSetLayoutCreateInfo{}
      .setBindings(skyboxBindings)
      .setBindingCount(1)
      .setFlags(
          vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  textureLayout =
      device.createDescriptorSetLayout(textureSetLayoutCreateInfo, nullptr);

  uniformLayout =
      device.createDescriptorSetLayout(uniformSetLayoutCreateInfo, nullptr);

  lightLayout =
      device.createDescriptorSetLayout(lightSetLayoutCreateInfo, nullptr);

  storageBufferLayout = device.createDescriptorSetLayout(
      storageBufferSetLayoutCreateInfo, nullptr);

  skyboxLayout = device.createDescriptorSetLayout(skyboxSetLayoutCreateInfo, nullptr);
}

Descriptor GPU::createStorageBufferDescriptor(
    const vk::DescriptorSetLayout& layout) {
  Descriptor descriptor{};
  descriptor.layout = layout;
  descriptor.type = vk::DescriptorType::eStorageBuffer;

  descriptor.size = utils::getAlignedSize(
      device.getDescriptorSetLayoutSizeEXT(layout, dld),
      descriptorBufferProperties.descriptorBufferOffsetAlignment);

  descriptor.buffer =
      createBuffer(descriptor.size,
                   vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                       vk::BufferUsageFlagBits::eShaderDeviceAddress,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_MAPPED_BIT |
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  descriptor.address.setDeviceAddress(
      getBufferDeviceAddress(descriptor.buffer));

  return descriptor;
};

Descriptor GPU::createUniformDescriptor(uint32_t count,
                                        const vk::DescriptorSetLayout& layout) {
  Descriptor descriptor{};
  descriptor.layout = layout;
  descriptor.type = vk::DescriptorType::eUniformBuffer;

  descriptor.size = utils::getAlignedSize(
      device.getDescriptorSetLayoutSizeEXT(layout, dld),
      descriptorBufferProperties.descriptorBufferOffsetAlignment);

  descriptor.buffer =
      createBuffer(descriptor.size * static_cast<uint32_t>(count),
                   vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                       vk::BufferUsageFlagBits::eShaderDeviceAddress,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_MAPPED_BIT |
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  descriptor.address.setDeviceAddress(
      getBufferDeviceAddress(descriptor.buffer));

  return descriptor;
};

Descriptor GPU::createTextureDescriptor(uint32_t count,
                                        const vk::DescriptorSetLayout& layout) {
  Descriptor descriptor{};
  descriptor.layout = layout;
  descriptor.type = vk::DescriptorType::eCombinedImageSampler;

  descriptor.size = utils::getAlignedSize(
      device.getDescriptorSetLayoutSizeEXT(layout, dld),
      descriptorBufferProperties.descriptorBufferOffsetAlignment);

  descriptor.buffer =
      createBuffer(descriptor.size * static_cast<uint32_t>(count),
                   vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                       vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT |
                       vk::BufferUsageFlagBits::eShaderDeviceAddress,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_MAPPED_BIT |
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  descriptor.address.setDeviceAddress(
      getBufferDeviceAddress(descriptor.buffer));

  return descriptor;
};

void GPU::setDescriptorStorageBuffer(const Descriptor& descriptor,
                                     const Buffer& src,
                                     uint32_t index,
                                     uint32_t binding) {
  char* storageBufferDescriptorPtr =
      reinterpret_cast<char*>(descriptor.buffer.allocationInfo.pMappedData);

  vk::DescriptorAddressInfoEXT storageBufferDescriptorAddressInfo =
      vk::DescriptorAddressInfoEXT{}
          .setRange(src.size)
          .setFormat(vk::Format::eUndefined)
          .setAddress(getBufferDeviceAddress(src));

  vk::DescriptorGetInfoEXT storageBufferDescriptorInfo =
      vk::DescriptorGetInfoEXT{}
          .setData(vk::DescriptorDataEXT{}.setPStorageBuffer(
              &storageBufferDescriptorAddressInfo))
          .setType(descriptor.type);

  vk::DeviceSize offset = getDescriptorBindingOffset(descriptor, binding);

  device.getDescriptorEXT(
      storageBufferDescriptorInfo,
      descriptorBufferProperties.storageBufferDescriptorSize,
      storageBufferDescriptorPtr + (index * descriptor.size) + offset, dld);
}

void GPU::setDescriptorUniformBuffer(const Descriptor& descriptor,
                                     const Buffer& src,
                                     uint32_t index,
                                     uint32_t binding) {
  char* uniformDescriptorPtr =
      reinterpret_cast<char*>(descriptor.buffer.allocationInfo.pMappedData);

  vk::DescriptorAddressInfoEXT uniformDescriptorAddressInfo =
      vk::DescriptorAddressInfoEXT{}
          .setRange(src.size)
          .setFormat(vk::Format::eUndefined)
          .setAddress(getBufferDeviceAddress(src));
  vk::DescriptorGetInfoEXT uniformDescriptorInfo =
      vk::DescriptorGetInfoEXT{}
          .setData(vk::DescriptorDataEXT{}.setPUniformBuffer(
              &uniformDescriptorAddressInfo))
          .setType(descriptor.type);

  vk::DeviceSize offset = getDescriptorBindingOffset(descriptor, binding);

  device.getDescriptorEXT(
      uniformDescriptorInfo,
      descriptorBufferProperties.uniformBufferDescriptorSize,
      uniformDescriptorPtr + (index * descriptor.size) + offset, dld);
}

void GPU::setDescriptorImage(const Descriptor& descriptor,
                             const Image& src,
                             const vk::Sampler& sampler,
                             uint32_t index,
                             uint32_t binding) {
  char* textureDescriptorPtr =
      reinterpret_cast<char*>(descriptor.buffer.allocationInfo.pMappedData);

  vk::DescriptorImageInfo textureProjectionDescriptorImageInfo =
      vk::DescriptorImageInfo{}
          .setSampler(sampler)
          .setImageView(src.view)
          .setImageLayout(src.layout);

  vk::DescriptorGetInfoEXT textureDescriptorInfo =
      vk::DescriptorGetInfoEXT{}
          .setData(vk::DescriptorDataEXT{}.setPCombinedImageSampler(
              &textureProjectionDescriptorImageInfo))
          .setType(descriptor.type);

  vk::DeviceSize offset = getDescriptorBindingOffset(descriptor, binding);

  device.getDescriptorEXT(
      textureDescriptorInfo,
      descriptorBufferProperties.combinedImageSamplerDescriptorSize,
      textureDescriptorPtr + (index * descriptor.size) + offset, dld);
}

vk::DeviceSize GPU::getDescriptorBindingOffset(const Descriptor& descriptor,
                                               uint32_t binding) {
  return device.getDescriptorSetLayoutBindingOffsetEXT(descriptor.layout,
                                                       binding, dld);
}

void GPU::destroyDescriptor(const Descriptor& descriptor) {
  vmaDestroyBuffer(allocator, descriptor.buffer.buffer,
                   descriptor.buffer.allocation);
}

Buffer GPU::createBuffer(const void* data,
                         const vk::DeviceSize size,
                         vk::Flags<vk::BufferUsageFlagBits> usage) {
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

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b,
                  &buffer.allocation, &buffer.allocationInfo);

  buffer.buffer = b;
  vmaCopyMemoryToAllocation(allocator, data, buffer.allocation, 0, size);

  return buffer;
}

Buffer GPU::createBuffer(const vk::DeviceSize size,
                         const vk::Flags<vk::BufferUsageFlagBits> usage) {
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

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b,
                  &buffer.allocation, &buffer.allocationInfo);

  buffer.buffer = b;
  return buffer;
}

Buffer GPU::createBuffer(const vk::DeviceSize size,
                         const vk::Flags<vk::BufferUsageFlagBits> usage,
                         VmaMemoryUsage memoryUsage,
                         VmaAllocationCreateFlags createFlags) {
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

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b,
                  &buffer.allocation, &buffer.allocationInfo);

  buffer.buffer = b;
  return buffer;
}

void GPU::copyBufferToImage(const Buffer& buffer,
                            const Image& image,
                            const vk::Extent2D& extent, const uint32_t layers) {
  std::vector<vk::BufferImageCopy2> copyRegions(layers);
  
  uint32_t offset = 0;

  for (size_t i = 0; i < layers; i++) {
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
    offset += (buffer.size / 6);
  }

  vk::CopyBufferToImageInfo2 copyInfo =
      vk::CopyBufferToImageInfo2{}
          .setSrcBuffer(buffer.buffer)
          .setDstImage(image.image)
          .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
          .setRegionCount(copyRegions.size())
          .setRegions(copyRegions);

  vk::CommandBuffer copyBufferToImageCmdBuffer = beginSingleSubmitCommand();
  copyBufferToImageCmdBuffer.copyBufferToImage2(copyInfo);
  endSingleSubmitCommand(copyBufferToImageCmdBuffer);
}

vk::DeviceAddress GPU::getBufferDeviceAddress(const Buffer& buffer) {
  vk::BufferDeviceAddressInfo bufferDeviceAddressInfo =
      vk::BufferDeviceAddressInfo{}.setBuffer(buffer.buffer);

  return device.getBufferAddress(bufferDeviceAddressInfo);
}

vk::CommandBuffer GPU::beginSingleSubmitCommand() {
  vk::CommandBuffer singleSubmitBuffer;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount(1)
          .setLevel(vk::CommandBufferLevel::ePrimary);

  if (device.allocateCommandBuffers(&commandBufferAllocateInfo,
                                    &singleSubmitBuffer) !=
      vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}.setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  singleSubmitBuffer.begin(beginInfo);

  return singleSubmitBuffer;
}

void GPU::endSingleSubmitCommand(const vk::CommandBuffer& singleSubmitBuffer) {
  singleSubmitBuffer.end();

  vk::SubmitInfo submitInfo = vk::SubmitInfo{}
                                  .setCommandBuffers(singleSubmitBuffer)
                                  .setCommandBufferCount(1);

  if (queue.submit(1, &submitInfo, nullptr) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to submit to queue"};
  };

  queue.waitIdle();
  device.freeCommandBuffers(commandPool, 1, &singleSubmitBuffer);
}

void GPU::createDepthImage() {
  Image image{};
  image.extent = vk::Extent3D{vkbSwapchain.extent}.setDepth(1);
  image.layout = vk::ImageLayout::eUndefined;

  VkImageCreateInfo imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(vk::Format::eD32Sfloat)
          .setMipLevels(1)
          .setArrayLayers(1)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
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
  depthImage = image;
}

Image GPU::createTexture2D(const uint8_t* data, const vk::Extent2D& extent) {
  Image image{};
  image.extent = vk::Extent3D{extent}.setDepth(1);
  image.layout = vk::ImageLayout::eUndefined;

  VkImageCreateInfo imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(vk::Format::eR8G8B8A8Srgb)
          .setMipLevels(1)
          .setArrayLayers(1)
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
          .setFormat(vk::Format::eR8G8B8A8Srgb)
          .setSubresourceRange(imageSubresourceRange);

  image.view = device.createImageView(imageViewCreateInfo, nullptr);
  uint32_t size = extent.width * extent.height * STBI_rgb_alpha;

  image.layout =
      transitionImageLayout(image, vk::ImageLayout::eTransferDstOptimal);
  Buffer stagingBuffer = createBuffer(
      data, size, vk::BufferUsageFlagBits::eTransferSrc);

  copyBufferToImage(stagingBuffer, image, extent);
  image.layout =
      transitionImageLayout(image, vk::ImageLayout::eShaderReadOnlyOptimal);

  destroyBuffer(stagingBuffer);

  return image;
}

Image GPU::createCubemapTexture(const std::array<uint8_t*, 6>& data, const vk::Extent2D& extent) {
  Image image{};
  image.extent = vk::Extent3D{extent}.setDepth(1);
  image.layout = vk::ImageLayout::eUndefined;

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

  vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo,
                 &vkImage, &image.allocation, nullptr);

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

  image.layout =
      transitionImageLayout(image, vk::ImageLayout::eTransferDstOptimal, 6);
  
  Buffer stagingBuffer = createBuffer(totalSize, vk::BufferUsageFlagBits::eTransferSrc);
  
  uint32_t offset = 0;
  for (const uint8_t* ptr : data) {
    vmaCopyMemoryToAllocation(allocator, ptr, stagingBuffer.allocation, offset, size);
    offset += size;
  }

  copyBufferToImage(stagingBuffer, image, extent, 6);

  image.layout =
      transitionImageLayout(image, vk::ImageLayout::eShaderReadOnlyOptimal, 6);

  destroyBuffer(stagingBuffer);

  return image;
}

vk::ImageLayout GPU::transitionImageLayout(const Image& image,
                                           const vk::ImageLayout& layout, const uint32_t layers) {
  vk::ImageSubresourceRange subresourceRange =
      vk::ImageSubresourceRange{}
          .setLayerCount(layers)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseMipLevel(0)
          .setLevelCount(1)
          .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 imageMemoryBarrier =
      vk::ImageMemoryBarrier2{}
          .setImage(image.image)
          .setOldLayout(image.layout)
          .setNewLayout(layout)
          .setSrcAccessMask(vk::AccessFlagBits2::eNone)
          .setDstAccessMask(vk::AccessFlagBits2::eNone)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
          .setDstStageMask(vk::PipelineStageFlagBits2::eNone)
          .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarriers[1] = {imageMemoryBarrier};

  vk::DependencyInfo dependencyInfo =
      vk::DependencyInfo{}
          .setImageMemoryBarriers(imageMemoryBarriers)
          .setImageMemoryBarrierCount(1);

  vk::CommandBuffer transitionImageLayoutCmdBuffer = beginSingleSubmitCommand();
  transitionImageLayoutCmdBuffer.pipelineBarrier2(dependencyInfo);
  endSingleSubmitCommand(transitionImageLayoutCmdBuffer);

  return layout;
}

void GPU::destroyBuffer(const Buffer& buffer) {
  vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void GPU::destroyImage(const Image& image) {
  device.destroyImageView(image.view);
  vmaDestroyImage(allocator, image.image, image.allocation);
}

void GPU::createViewportAndScissors() {
  vk::Extent2D extent = vk::Extent2D{vkbSwapchain.extent};

  viewport = vk::Viewport{}
                 .setWidth(extent.width)
                 .setHeight(extent.height)
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
  createDepthImage();
  createViewportAndScissors();

  vkb::destroy_swapchain(old);
}

void GPU::destroySwapchainResources() {
  for (const vk::ImageView imageView : swapchainImageViews) {
    device.destroyImageView(imageView);
  }

  device.destroyImageView(depthImage.view);
  vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
}

Shader GPU::loadShader(const std::string_view path,
                       vk::ShaderStageFlagBits stage) {
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

void GPU::destroyShader(const Shader& shader) {
  device.destroyShaderModule(shader.module);
}

Pipeline GPU::createPipeline(
    const Shader& vertexShader,
    const Shader& fragmentShader,
    std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts) {
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
          .setFormat(vk::Format::eR32G32B32Sfloat);

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
          .setOffset(offsetof(Vertex, normals))
          .setFormat(vk::Format::eR32G32B32Sfloat);

  std::vector<vk::VertexInputAttributeDescription> inputAttributes = {
      vertexPositionAttributeDescription,
      vertexColorAttributeDescription,
      vertexTextureCoordAttributeDescription,
      vertexNormalsAttributeDescription,
  };

  vk::PipelineShaderStageCreateInfo vertexShaderStage =
      vk::PipelineShaderStageCreateInfo{}
          .setStage(vk::ShaderStageFlagBits::eVertex)
          .setModule(vertexShader.module)
          .setPName("main")
          .setPSpecializationInfo(nullptr);

  vk::PipelineShaderStageCreateInfo fragmentShaderStage =
      vk::PipelineShaderStageCreateInfo{}
          .setStage(vk::ShaderStageFlagBits::eFragment)
          .setModule(fragmentShader.module)
          .setPName("main")
          .setPSpecializationInfo(nullptr);

  vk::Format colorAttachmentFormat = vk::Format::eB8G8R8A8Srgb;
  vk::Format depthAttachmentFormat = vk::Format::eD32Sfloat;

  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo =
      vk::PipelineRenderingCreateInfo{}
          .setColorAttachmentCount(1)
          .setDepthAttachmentFormat(depthAttachmentFormat)
          .setColorAttachmentFormats(colorAttachmentFormat);

  std::vector<vk::PipelineShaderStageCreateInfo> stages{
      vertexShaderStage,
      fragmentShaderStage,
  };

  vk::PushConstantRange pushConstantRange =
      vk::PushConstantRange{}
          .setOffset(0)
          .setSize(sizeof(FrameData))
          .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                         vk::ShaderStageFlagBits::eFragment);

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
          .setDepthBiasEnable(0)
          .setPolygonMode(vk::PolygonMode::eFill)
          .setCullMode(vk::CullModeFlagBits::eBack)
          .setFrontFace(vk::FrontFace::eCounterClockwise)
          .setLineWidth(1.0f);

  vk::PipelineMultisampleStateCreateInfo multisampleState =
      vk::PipelineMultisampleStateCreateInfo{}
          .setRasterizationSamples(vk::SampleCountFlagBits::e1)
          .setSampleShadingEnable(0);

  vk::PipelineDepthStencilStateCreateInfo depthStencilState =
      vk::PipelineDepthStencilStateCreateInfo{}
          .setDepthTestEnable(1)
          .setDepthWriteEnable(1)
          .setStencilTestEnable(0)
          .setDepthBoundsTestEnable(0)
          .setDepthCompareOp(vk::CompareOp::eLessOrEqual);

  vk::PipelineColorBlendAttachmentState colorBlendAttachmentState =
      vk::PipelineColorBlendAttachmentState{}
          .setBlendEnable(0)
          .setColorWriteMask(
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

  vk::PipelineColorBlendStateCreateInfo colorBlendState =
      vk::PipelineColorBlendStateCreateInfo{}
          .setAttachmentCount(1)
          .setAttachments(colorBlendAttachmentState);

  vk::DynamicState dynamicStates[2] = {vk::DynamicState::eViewport,
                                       vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamicState =
      vk::PipelineDynamicStateCreateInfo{}
          .setDynamicStates(dynamicStates)
          .setDynamicStateCount(2);

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
          .setLayout(pipeline.layout)
          .setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);

  vk::ResultValue<vk::Pipeline> pipelineResult =
      device.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);

  if (pipelineResult.result != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to create a pipeline"};
  }

  pipeline.vertexShader = vertexShader;
  pipeline.fragmentShader = fragmentShader;
  pipeline.pipeline = pipelineResult.value;

  return pipeline;
}

void GPU::destroyPipeline(const Pipeline& pipeline) {
  destroyShader(pipeline.vertexShader);
  destroyShader(pipeline.fragmentShader);
  device.destroyPipelineLayout(pipeline.layout);
  device.destroyPipeline(pipeline.pipeline);
}

void GPU::waitForFence() {
  if (device.waitForFences(1, &fence, 1, UINT64_MAX) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to wait for fence"};
  };
}

int32_t GPU::acquireNextImage() {
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
      throw std::runtime_error{"Failed to acquire next image"};
      break;
  }

  return static_cast<int32_t>(imageIndex);
}

void GPU::resetFence() {
  if (device.resetFences(1, &fence) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to reset fence"};
  };
}

void GPU::beginRendering(uint32_t imageIndex) {
  vk::ImageView swapImageView = swapchainImageViews[imageIndex];
  vk::Image swapImage = swapchainImages[imageIndex];

  commandBuffer.reset();

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}.setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);

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

  vk::RenderingAttachmentInfo attachment =
      vk::RenderingAttachmentInfo{}
          .setImageView(swapImageView)
          .setResolveMode(vk::ResolveModeFlagBits::eNone)
          .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setClearValue(clearValue);

  vk::RenderingInfo renderingInfo = vk::RenderingInfo{}
                                        .setRenderArea(scissors)
                                        .setLayerCount(1)
                                        .setViewMask(0)
                                        .setColorAttachmentCount(1)
                                        .setColorAttachments(attachment)
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

  vk::ImageMemoryBarrier2 imageMemoryBarriers[3] = {
      depthMemoryBarrier, imageMemoryBarrier, presentImageMemoryBarrier};

  vk::DependencyInfo dependencyInfo =
      vk::DependencyInfo{}
          .setImageMemoryBarriers(imageMemoryBarriers)
          .setImageMemoryBarrierCount(3);

  commandBuffer.pipelineBarrier2(dependencyInfo);
  commandBuffer.beginRendering(renderingInfo);
}
