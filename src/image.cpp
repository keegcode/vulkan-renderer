#include "image.hpp"
#include "buffer.hpp"
#include "stb_image.h"
#include "vk-utils.hpp"

Image::Image() {
}

Image::Image(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Extent3D& extent, const vk::Format format, const vk::ImageUsageFlagBits usage, const vk::ImageAspectFlagBits aspectMask) {
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

  vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo, &vkImage, &allocation, nullptr);
  
  image = vkImage;

  vk::ImageSubresourceRange imageSubresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(aspectMask)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo = vk::ImageViewCreateInfo{}
      .setImage(image)
      .setViewType(vk::ImageViewType::e2D)
      .setFormat(format)
      .setSubresourceRange(imageSubresourceRange);
  
  view = device.createImageView(imageViewCreateInfo, nullptr);
}

Image::Image(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const std::string_view path, vk::ImageLayout l) {
  int height, width;

  unsigned char* data = stbi_load(path.data(), reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height), nullptr, STBI_rgb_alpha);
  
  extent = vk::Extent3D{}
      .setWidth(width)
      .setHeight(height)
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

  vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo, &vkImage, &allocation, nullptr);

  image = vkImage;

  vk::ImageSubresourceRange imageSubresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageViewCreateInfo imageViewCreateInfo = vk::ImageViewCreateInfo{}
      .setImage(image)
      .setViewType(vk::ImageViewType::e2D)
      .setFormat(vk::Format::eR8G8B8A8Srgb)
      .setSubresourceRange(imageSubresourceRange);
  
  view = device.createImageView(imageViewCreateInfo, nullptr);

  uint32_t size = width * height * STBI_rgb_alpha;
  Buffer stagingBuffer{allocator, data, size, vk::BufferUsageFlagBits::eTransferSrc};
  
  transitionImageLayout(device, commandPool, transferQueue, vk::ImageLayout::eTransferDstOptimal);
  stagingBuffer.copyToImage(allocator, device, commandPool, transferQueue, image, data, extent);
  transitionImageLayout(device, commandPool, transferQueue, l);

  stbi_image_free(data);
};

void Image::transitionImageLayout(const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::ImageLayout& newLayout) {
  vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 imageMemoryBarrier = vk::ImageMemoryBarrier2{}
    .setImage(image)
    .setOldLayout(layout)
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

  vk::CommandBuffer commandBuffer = utils::beginSingleSubmitCommand(device, commandPool);
  
  commandBuffer.pipelineBarrier2(dependencyInfo);

  utils::endSingleSubmitCommand(device, commandPool, commandBuffer, transferQueue);

  layout = newLayout;
}

void Image::destroy(const VmaAllocator& allocator, const vk::Device& device) {
  device.destroyImageView(view);
  vmaDestroyImage(allocator, image, allocation);
}
