#include "utils.hpp"

#include <fstream>
#include "VkBootstrap.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

vkb::Swapchain utils::createSwapchain(vkb::Device device,
                                      vk::Extent2D extent,
                                      uint16_t minImageCount,
                                      vkb::Swapchain* old) {
  vkb::SwapchainBuilder builder =
      vkb::SwapchainBuilder{device}
          .set_old_swapchain(*old)
          .set_desired_extent(extent.width, extent.height)
          .set_required_min_image_count(minImageCount)
          .set_desired_present_mode(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR);

  if (old) {
    builder.set_old_swapchain(*old);
  }

  vkb::Result<vkb::Swapchain> swapchainResult = builder.build();

  if (!swapchainResult) {
    throw std::runtime_error{"Failed to create a swapchain: " +
                             swapchainResult.error().message()};
  }

  return swapchainResult.value();
};

std::tuple<vk::Viewport, vk::Rect2D> utils::createViewportAndScissors(
    const vk::Extent3D& extent) {
  vk::Viewport viewport = vk::Viewport{}
                              .setX(0)
                              .setY(0)
                              .setWidth(extent.width)
                              .setHeight(extent.height)
                              .setMaxDepth(1.0f)
                              .setMinDepth(0.0f);

  vk::Rect2D scissors =
      vk::Rect2D{}
          .setExtent(vk::Extent2D{extent.width, extent.height})
          .setOffset(0);

  return std::make_tuple(viewport, scissors);
};

vk::CommandBuffer utils::beginSingleSubmitCommand(
    const vk::Device& device,
    const vk::CommandPool& commandPool) {
  vk::CommandBuffer commandBuffer;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount(1)
          .setLevel(vk::CommandBufferLevel::ePrimary);

  if (device.allocateCommandBuffers(&commandBufferAllocateInfo,
                                    &commandBuffer) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}.setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);

  return commandBuffer;
}

void utils::endSingleSubmitCommand(const vk::Device& device,
                                   const vk::CommandPool& commandPool,
                                   const vk::CommandBuffer& commandBuffer,
                                   const vk::Queue& queue) {
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

vk::DeviceSize utils::getAlignedSize(const vk::DeviceSize size,
                                     const vk::DeviceSize alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

std::vector<char> utils::readFile(const std::string_view path) {
  std::ifstream file{path.data(),
                     std::ios::in | std::ios::binary | std::ios::ate};

  if (!file.is_open()) {
    throw std::runtime_error{std::string{"Failed to open file: "} +
                             path.data()};
  }

  std::ifstream::pos_type size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> bytes(size);
  file.read(bytes.data(), size);

  return bytes;
}
