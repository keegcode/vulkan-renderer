#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include "vk-utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

vkb::Swapchain utils::createSwapchain(vkb::Device device, vk::Extent2D extent, uint16_t minImageCount) {
  vkb::Result<vkb::Swapchain> swapchainResult = vkb::SwapchainBuilder{device}
    .set_desired_extent(extent.width, extent.height)
    .set_required_min_image_count(minImageCount)
    .set_desired_present_mode(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
    .build();
  
  if (!swapchainResult) {
    throw std::runtime_error{"Failed to create a swapchain: " + swapchainResult.error().message()};
  }

  return swapchainResult.value(); 
};

Image utils::createImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlagBits usage, vk::ImageAspectFlagBits aspectMask) {
  Image image{};

  VkImageCreateInfo imageCreateInfo = vk::ImageCreateInfo{}
    .setImageType(vk::ImageType::e2D)
    .setFormat(format)
    .setMipLevels(1)
    .setArrayLayers(1)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setTiling(vk::ImageTiling::eOptimal)
    .setUsage(usage)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setExtent(extent);

  VmaAllocationCreateInfo imageAllocationCreateInfo{};
  imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  imageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  VkImage vkImage;
  VmaAllocation allocation;

  vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo, &vkImage, &allocation, nullptr);

  image.image = vkImage;
  image.allocation = allocation;

  vk::ImageSubresourceRange imageSubresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(aspectMask)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo = vk::ImageViewCreateInfo{}
      .setImage(image.image)
      .setViewType(vk::ImageViewType::e2D)
      .setFormat(format)
      .setSubresourceRange(imageSubresourceRange);
  
  image.imageView = device.createImageView(imageViewCreateInfo, nullptr);

  return image;
}

Image utils::createTextureImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const std::string_view path, vk::ImageLayout layout) {
  Image image{};

  unsigned char* data = stbi_load(path.data(), reinterpret_cast<int*>(&image.extent.width), reinterpret_cast<int*>(&image.extent.height), nullptr, STBI_rgb_alpha);
  
  vk::Extent3D extent = vk::Extent3D{}
      .setWidth(image.extent.width)
      .setHeight(image.extent.height)
      .setDepth(1);

  VkImageCreateInfo imageCreateInfo = vk::ImageCreateInfo{}
    .setImageType(vk::ImageType::e2D)
    .setFormat(vk::Format::eR8G8B8A8Srgb)
    .setMipLevels(1)
    .setArrayLayers(1)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setTiling(vk::ImageTiling::eOptimal)
    .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setExtent(extent);

  VmaAllocationCreateInfo imageAllocationCreateInfo{};
  imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  imageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  VkImage vkImage;
  VmaAllocation allocation;

  vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo, &vkImage, &allocation, nullptr);

  image.image = vkImage;
  image.allocation = allocation;

  vk::ImageSubresourceRange imageSubresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo = vk::ImageViewCreateInfo{}
      .setImage(image.image)
      .setViewType(vk::ImageViewType::e2D)
      .setFormat(vk::Format::eR8G8B8A8Srgb)
      .setSubresourceRange(imageSubresourceRange);
  
  image.imageView = device.createImageView(imageViewCreateInfo, nullptr);
  
  utils::transitionImageLayout(device, commandPool, transferQueue, image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  utils::copyBufferToImage(allocator, device, commandPool, transferQueue, image.image, data, extent);
  utils::transitionImageLayout(device, commandPool, transferQueue, image.image, vk::ImageLayout::eUndefined, layout);

  stbi_image_free(data);

  return image;
};

std::tuple<vk::Viewport, vk::Rect2D> utils::createViewportAndScissors(const vk::Extent3D& extent) {
  vk::Viewport viewport = vk::Viewport{}
    .setX(0)
    .setY(0)
    .setWidth(extent.width)
    .setHeight(extent.height)
    .setMaxDepth(1.0f)
    .setMinDepth(0.0f);

  vk::Rect2D scissors = vk::Rect2D{}
    .setExtent(vk::Extent2D{extent.width, extent.height})
    .setOffset(0);

  return std::make_tuple(viewport, scissors);
};

Buffer utils::createBuffer(VmaAllocator allocator, const void* data, const uint32_t size, vk::BufferUsageFlagBits usage) {
  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(size)
    .setUsage(usage)
    .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer buffer;
  VmaAllocation allocation;

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &buffer, &allocation, nullptr);
  vmaCopyMemoryToAllocation(allocator, data, allocation, 0, size);

  return Buffer{buffer,allocation};
};
vk::CommandBuffer utils::beginSingleSubmitCommand(const vk::Device& device, const vk::CommandPool& commandPool) {
  vk::CommandBuffer commandBuffer;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{}
    .setCommandPool(commandPool)
    .setCommandBufferCount(1)
    .setLevel(vk::CommandBufferLevel::ePrimary);

  if (device.allocateCommandBuffers(&commandBufferAllocateInfo, &commandBuffer) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}
    .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);

  return commandBuffer;
}

void utils::endSingleSubmitCommand(const vk::Device& device, const vk::CommandPool& commandPool, const vk::CommandBuffer& commandBuffer, const vk::Queue& queue) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo = vk::SubmitInfo{}
    .setCommandBuffers(commandBuffer)
    .setCommandBufferCount(1);

  if (queue.submit(1, &submitInfo, nullptr) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to submit to queue"};
  };

  queue.waitIdle();
  device.freeCommandBuffers(commandPool, 1, &commandBuffer);
}

void utils::transitionImageLayout(const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Image& image, const vk::ImageLayout& oldLayout, const vk::ImageLayout& newLayout) {
  vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 imageMemoryBarrier = vk::ImageMemoryBarrier2{}
    .setImage(image)
    .setOldLayout(oldLayout)
    .setNewLayout(newLayout)
    .setSrcAccessMask(vk::AccessFlagBits2::eNone)
    .setDstAccessMask(vk::AccessFlagBits2::eNone)
    .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
    .setDstStageMask(vk::PipelineStageFlagBits2::eNone)
    .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarriers[1] = {imageMemoryBarrier};

  vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}
    .setImageMemoryBarriers(imageMemoryBarriers)
    .setImageMemoryBarrierCount(1);

  vk::CommandBuffer commandBuffer = beginSingleSubmitCommand(device, commandPool);
  
  commandBuffer.pipelineBarrier2(dependencyInfo);

  endSingleSubmitCommand(device, commandPool, commandBuffer, transferQueue);
}

void utils::copyBufferToImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Image& image, unsigned char* srcData, const vk::Extent3D& extent) {
  vk::DeviceSize size = extent.width * extent.height * sizeof(unsigned char) * STBI_rgb_alpha;
  
  Buffer stagingBuffer = utils::createBuffer(allocator, srcData, size, vk::BufferUsageFlagBits::eTransferSrc);

  vk::ImageSubresourceLayers imageSubresourceLayers  = vk::ImageSubresourceLayers{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseArrayLayer(0)
    .setMipLevel(0);

  vk::BufferImageCopy2 copyRegion = vk::BufferImageCopy2{}
    .setImageOffset(0)
    .setBufferOffset(0)
    .setBufferRowLength(0)
    .setBufferImageHeight(0)
    .setImageExtent(extent)
    .setImageSubresource(imageSubresourceLayers);

  vk::CopyBufferToImageInfo2 copyInfo = vk::CopyBufferToImageInfo2{}
    .setSrcBuffer(stagingBuffer.buffer)
    .setDstImage(image)
    .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
    .setRegionCount(1)
    .setRegions(copyRegion);

  vk::CommandBuffer commandBuffer = beginSingleSubmitCommand(device, commandPool);

  commandBuffer.copyBufferToImage2(copyInfo);

  endSingleSubmitCommand(device, commandPool, commandBuffer, transferQueue);

  vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}
