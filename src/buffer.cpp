#include "buffer.hpp"
#include <cstdint>
#include "stb_image.h"
#include "utils.hpp"

Buffer::Buffer() {}

Buffer::Buffer(const VmaAllocator& allocator,
               const void* data,
               const vk::DeviceSize s,
               vk::Flags<vk::BufferUsageFlagBits> usage)
    : size{s} {
  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  bufferAllocationCreateInfo.flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo =
      vk::BufferCreateInfo{}.setSize(size).setUsage(usage).setSharingMode(
          vk::SharingMode::eExclusive);

  VkBuffer b;

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b,
                  &allocation, &allocationInfo);

  vmaCopyMemoryToAllocation(allocator, data, allocation, 0, size);

  buffer = b;
}

Buffer::Buffer(const VmaAllocator& allocator,
               const vk::DeviceSize s,
               const vk::Flags<vk::BufferUsageFlagBits> usage)
    : size{s} {
  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  bufferAllocationCreateInfo.flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo bufferCreateInfo =
      vk::BufferCreateInfo{}.setSize(size).setUsage(usage).setSharingMode(
          vk::SharingMode::eExclusive);

  VkBuffer b;

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b,
                  &allocation, &allocationInfo);

  buffer = b;
}

Buffer::Buffer(const VmaAllocator& allocator,
               const vk::DeviceSize s,
               const vk::Flags<vk::BufferUsageFlagBits> usage,
               VmaMemoryUsage memoryUsage,
               VmaAllocationCreateFlags createFlags)
    : size{s} {
  VmaAllocationCreateInfo bufferAllocationCreateInfo{};
  bufferAllocationCreateInfo.usage = memoryUsage;
  bufferAllocationCreateInfo.flags = createFlags;

  VkBufferCreateInfo bufferCreateInfo =
      vk::BufferCreateInfo{}.setSize(size).setUsage(usage).setSharingMode(
          vk::SharingMode::eExclusive);

  VkBuffer b;

  vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &b,
                  &allocation, &allocationInfo);

  buffer = b;
}

void Buffer::copyToImage(const VmaAllocator& allocator,
                         const vk::Device& device,
                         const vk::CommandPool& commandPool,
                         const vk::Queue& transferQueue,
                         const vk::Image& image,
                         uint8_t* srcData,
                         vk::DeviceSize size,
                         const vk::Extent3D& extent) {
  Buffer stagingBuffer =
      Buffer{allocator, srcData, size, vk::BufferUsageFlagBits::eTransferSrc};

  vk::ImageSubresourceLayers imageSubresourceLayers =
      vk::ImageSubresourceLayers{}
          .setLayerCount(1)
          .setAspectMask(vk::ImageAspectFlagBits::eColor)
          .setBaseArrayLayer(0)
          .setMipLevel(0);

  vk::BufferImageCopy2 copyRegion =
      vk::BufferImageCopy2{}
          .setImageOffset(0)
          .setBufferOffset(0)
          .setBufferRowLength(0)
          .setBufferImageHeight(0)
          .setImageExtent(extent)
          .setImageSubresource(imageSubresourceLayers);

  vk::CopyBufferToImageInfo2 copyInfo =
      vk::CopyBufferToImageInfo2{}
          .setSrcBuffer(stagingBuffer.buffer)
          .setDstImage(image)
          .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
          .setRegionCount(1)
          .setRegions(copyRegion);

  vk::CommandBuffer commandBuffer =
      utils::beginSingleSubmitCommand(device, commandPool);

  commandBuffer.copyBufferToImage2(copyInfo);

  utils::endSingleSubmitCommand(device, commandPool, commandBuffer,
                                transferQueue);

  stagingBuffer.destroy(allocator);
}

void Buffer::destroy(const VmaAllocator& allocator) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}

vk::DeviceAddress Buffer::getDeviceAddress(const vk::Device& device) const {
  vk::BufferDeviceAddressInfo bufferDeviceAddressInfo =
      vk::BufferDeviceAddressInfo{}.setBuffer(buffer);

  return device.getBufferAddress(bufferDeviceAddressInfo);
}
