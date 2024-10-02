#include "stb_image.h"
#include "buffer.hpp"
#include "vk-utils.hpp"

Buffer::Buffer() {
}

Buffer::Buffer(const VmaAllocator& allocator, const void* data, const uint32_t s, vk::BufferUsageFlagBits usage): size{s} {
  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(size)
    .setUsage(usage)
    .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer b;

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b, &allocation, nullptr);
  vmaCopyMemoryToAllocation(allocator, data, allocation, 0, size);

  buffer = b;
};

Buffer::Buffer(const VmaAllocator& allocator, const uint32_t s, const vk::BufferUsageFlagBits usage): size{s} {
  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(size)
    .setUsage(usage)
    .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer b;

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b, &allocation, nullptr);

  buffer = b;
}

void Buffer::copyToImage(const VmaAllocator& allocator, const vk::Device& device, const vk::CommandPool& commandPool, const vk::Queue& transferQueue, const vk::Image& image, unsigned char* srcData, const vk::Extent3D& extent) {
  Buffer stagingBuffer = Buffer{allocator, srcData, size, vk::BufferUsageFlagBits::eTransferSrc};

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

  vk::CommandBuffer commandBuffer = utils::beginSingleSubmitCommand(device, commandPool);

  commandBuffer.copyBufferToImage2(copyInfo);

  utils::endSingleSubmitCommand(device, commandPool, commandBuffer, transferQueue);

  vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
};

void Buffer::destroy(const VmaAllocator& allocator) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}
