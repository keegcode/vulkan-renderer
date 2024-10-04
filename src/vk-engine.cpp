#include "vk-engine.hpp"
#include "VkBootstrap.h"
#include "image.hpp"
#include "object.hpp"
#include "scene.hpp"
#include "vk-pipeline.hpp"
#include "vk-utils.hpp"

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_video.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>  
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

void VkEngine::init(const Display& d) {
  display = d;
  createInstance();
  pickPhysicalDevice();
  pickDevice();
  createSwapchain();
  createAllocator();
  createQueue();
  createSyncPrimitives();
  createCommandPool();
  createCommandBuffers();
  createSampler();
  createDescriptorPool();
  createDepthImage();
  createViewportAndScissors();
  createPipelines();
};
  
void VkEngine::destroySwapchainResources() {
  vk::Device d = device.device;

  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    d.destroyImageView(swapchainImageViews[i]);
  }

  d.destroyImageView(depthImage.view);
  vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
}

void VkEngine::rebuiltSwapchain() {
  vk::Device d = device.device;
  vkb::Swapchain old = swapchain;

  d.waitIdle();

  destroySwapchainResources();

  createSwapchain();
  createDepthImage();
  createViewportAndScissors();

  vkb::destroy_swapchain(old);
}

void VkEngine::drawFrame(float deltaTime) {
  vk::Device d = device.device;

  if (d.waitForFences(1, &fences[frame], 1, UINT64_MAX) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to wait for fence"};
  };

  if (shouldBeResized) {
    rebuiltSwapchain();
    shouldBeResized = false;
  }

  vk::SwapchainKHR swap = swapchain.swapchain;
  
  uint32_t imageIndex;
  vk::Result acquireResult = d.acquireNextImageKHR(swap, UINT64_MAX, presentCompleteSemaphores[frame], nullptr, &imageIndex);

  vk::ImageView swapImageView = swapchainImageViews[imageIndex];
  vk::Image swapImage = swapchainImages[imageIndex];
  
  switch (acquireResult) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eSuboptimalKHR:
      rebuiltSwapchain();
      return;
    case vk::Result::eErrorOutOfDateKHR:
      rebuiltSwapchain();
      return;
    case vk::Result::eNotReady:
    default:
      throw std::runtime_error{"Failed to acquire next image"};
      break;
  }

  if (d.resetFences(1, &fences[frame]) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to reset fence"};
  };

  vk::CommandBuffer commandBuffer = commadBuffers[frame];

  commandBuffer.reset();

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}
    .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);

  vk::ClearValue clearValue = vk::ClearValue{}.setColor(vk::ClearColorValue{}.setUint32({0xFF, 0XFF, 0xFF, 0xFF}));
  vk::ClearValue depthClearValue = vk::ClearValue{}.setDepthStencil(vk::ClearDepthStencilValue{}.setDepth(1.0f).setStencil(0));

  vk::RenderingAttachmentInfo depthAttachment = vk::RenderingAttachmentInfo{}
    .setImageView(depthImage.view)
    .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eNone)
    .setClearValue(depthClearValue);

  vk::RenderingAttachmentInfo attachment = vk::RenderingAttachmentInfo{}
    .setImageView(swapImageView)
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

  vk::ImageSubresourceRange depthSubresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eDepth)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 depthMemoryBarrier = vk::ImageMemoryBarrier2{}
    .setImage(depthImage.image)
    .setOldLayout(vk::ImageLayout::eUndefined)
    .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
    .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
    .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
    .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
    .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
    .setSubresourceRange(depthSubresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarrier = vk::ImageMemoryBarrier2{}
    .setImage(swapImage)
    .setOldLayout(vk::ImageLayout::eUndefined)
    .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
    .setSrcAccessMask(vk::AccessFlagBits2::eNone)
    .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
    .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
    .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
    .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 presentImageMemoryBarrier = vk::ImageMemoryBarrier2{}
    .setImage(swapImage)
    .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
    .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
    .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
    .setDstAccessMask(vk::AccessFlagBits2::eNone)
    .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
    .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
    .setSubresourceRange(subresourceRange);

  vk::ImageMemoryBarrier2 imageMemoryBarriers[3] = {depthMemoryBarrier, imageMemoryBarrier, presentImageMemoryBarrier};

  vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}
    .setImageMemoryBarriers(imageMemoryBarriers)
    .setImageMemoryBarrierCount(3);
  
  commandBuffer.pipelineBarrier2(dependencyInfo);

  commandBuffer.beginRendering(renderingInfo);

  commandBuffer.setViewport(0, 1, &viewport);
  commandBuffer.setScissor(0, 1, &scissors);

  vk::DeviceSize offsets[1] = {0};

  for (Object& object : objects) {
    const Mesh& mesh = meshes[object.meshIdx];
    const Texture& texture = textures[object.textureIdx];
    const Pipeline& pipeline = pipelines[object.pipelineIdx];

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipeline);

    std::vector<vk::DescriptorSet> sets{texture.descriptorSets[frame], pipeline.descriptorSets[frame], object.descriptorSets[frame]};
    
    projection.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
    
    object.uniform.rotation = glm::rotate(object.uniform.rotation, glm::radians(0.1f) * deltaTime, glm::vec3{0.0f, 1.0f, 0.0f});

    vmaCopyMemoryToAllocation(allocator, &projection, pipeline.projection.allocation, 0, sizeof(Projection));
    vmaCopyMemoryToAllocation(allocator, &object.uniform, object.ubo.allocation, 0, sizeof(UniformBuffer));

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);
    commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);
    commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0, vk::IndexType::eUint16);
    commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
  }

  commandBuffer.endRendering();

  commandBuffer.end();

  vk::Flags<vk::PipelineStageFlagBits> waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  
  vk::SubmitInfo submitInfo = vk::SubmitInfo{}
    .setWaitSemaphoreCount(1)
    .setWaitSemaphores(presentCompleteSemaphores[frame])
    .setCommandBuffers(commandBuffer)
    .setCommandBufferCount(1)
    .setSignalSemaphores(renderCompleteSemaphores[frame])
    .setSignalSemaphoreCount(1)
    .setWaitDstStageMask(waitStage);

  if (queue.submit(1, &submitInfo, fences[frame]) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to submit to queue"};
  };

  uint32_t imageIndices = {imageIndex};

  vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR{}
    .setWaitSemaphoreCount(1)
    .setWaitSemaphores(renderCompleteSemaphores[frame])
    .setSwapchainCount(1)
    .setSwapchains(swap)
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
      throw std::runtime_error{"Not ready swapchain"};
      break;
    default:
      throw std::runtime_error{"Failed to presentKHR"};
      break;
  }

  frame = (frame + 1) % MAX_CONCURRENT_FRAMES;
};

void VkEngine::processInput(float deltaTime) {
  SDL_Event event;
  while(SDL_PollEvent(&event)) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
      isRunning = false;
      break;
    }
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
      isRunning = false;
      break;
    }
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
      shouldBeResized = true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
      SDL_SetRelativeMouseMode(SDL_TRUE);
      SDL_SetWindowGrab(display.window, SDL_TRUE);
      camera.mode = CameraMode::Move; 
    }
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_SetWindowGrab(display.window, SDL_FALSE);
      camera.mode = CameraMode::Fixed; 
    }
    if (event.type == SDL_KEYDOWN && camera.mode == CameraMode::Move) {
      switch (event.key.keysym.sym) {
        case SDLK_w:
          camera.pos += camera.front * camera.velocity * deltaTime;
          break;
        case SDLK_s:
          camera.pos -= camera.front * camera.velocity * deltaTime;
          break;
        case SDLK_a:
          camera.pos -= camera.right * camera.velocity * deltaTime;
          break;
        case SDLK_d:
          camera.pos += camera.right * camera.velocity * deltaTime;
          break;
        default:
          break;
      }
    }
    if (event.type == SDL_MOUSEMOTION && camera.mode == CameraMode::Move) {
      camera.yaw += event.motion.xrel * camera.sensitivity;
      camera.pitch += -event.motion.yrel * camera.sensitivity;

      camera.pitch = std::clamp(camera.pitch, -90.0f, 90.0f);

      glm::vec3 front{0.0f};

      front.x = glm::cos(glm::radians(camera.yaw)) * glm::cos(glm::radians(camera.pitch));
      front.y = glm::sin(glm::radians(camera.pitch));
      front.z = glm::sin(glm::radians(camera.yaw)) * glm::cos(glm::radians(camera.pitch));
      
      camera.front = glm::normalize(front);
      camera.right = glm::normalize(glm::cross(camera.front, camera.up));
    }
  }
};

void VkEngine::destroy() {
  vk::Device d = device.device;

  d.waitIdle();
    
  while (!deleteQueue.empty()) {
    deleteQueue.back()();
    deleteQueue.pop_back();
  }
  
  destroySwapchainResources();
  vkb::destroy_swapchain(swapchain);
  vkb::destroy_surface(instance, surface);
  vmaDestroyAllocator(allocator);
  vkb::destroy_device(device);
  vkb::destroy_instance(instance);
};

void VkEngine::createInstance() {
  vkb::Result<vkb::Instance> instanceResult = vkb::InstanceBuilder{}
    .set_app_name("VkRenderer")
    .require_api_version(1, 3)
    .enable_extensions(display.vulkanExtensions)
    .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
    .enable_validation_layers(true)
    .use_default_debug_messenger()
    .build();
  
  if (!instanceResult) {
    throw std::runtime_error{"Failed to create instance: " + instanceResult.error().message()};
  }

  instance = instanceResult.value();
};

void VkEngine::pickPhysicalDevice() {
  surface = display.createVulkanSurface(instance.instance);

  vkb::Result<vkb::PhysicalDevice> physicalDeviceResult = vkb::PhysicalDeviceSelector{instance}
    .set_surface(surface)
    .set_minimum_version(1, 3)
    .require_present(true)
    .add_required_extension_features(vk::PhysicalDeviceDynamicRenderingFeatures{}.setDynamicRendering(1))
    .add_required_extension_features(vk::PhysicalDeviceSynchronization2Features{}.setSynchronization2(1))
    .select();

  if (!physicalDeviceResult) {
    throw std::runtime_error{"Failed to select physical device: " + physicalDeviceResult.error().message()};
  }

  physicalDevice = physicalDeviceResult.value();
};

void VkEngine::pickDevice() {
  vkb::Result<vkb::Device> deviceResult = vkb::DeviceBuilder{physicalDevice}
    .build();

  if (!deviceResult) {
    throw std::runtime_error{"Failed to create a device: " + deviceResult.error().message()};
  }

  device = deviceResult.value();
};

void VkEngine::createAllocator() {
  VmaVulkanFunctions vulkanFunctions{};
  vulkanFunctions.vkGetDeviceProcAddr = instance.fp_vkGetDeviceProcAddr;
  vulkanFunctions.vkGetInstanceProcAddr = instance.fp_vkGetInstanceProcAddr;

  VmaAllocatorCreateInfo createInfo{};
  createInfo.pVulkanFunctions = &vulkanFunctions;
  createInfo.instance = instance.instance;
  createInfo.physicalDevice = physicalDevice.physical_device;
  createInfo.device = device.device;
 
  if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create a VmaAllocator"};
  };
};

void VkEngine::createSwapchain() {
  int w, h;
  SDL_GetWindowSize(display.window, &w, &h);

  vk::Extent2D extent = vk::Extent2D{}
    .setWidth(w)
    .setHeight(h);

  swapchain = utils::createSwapchain(device, extent, MAX_CONCURRENT_FRAMES, &swapchain);
  
  vkb::Result<std::vector<VkImageView>> imageViewsResult = swapchain.get_image_views();
  vkb::Result<std::vector<VkImage>> imagesResult = swapchain.get_images();

  if (!imageViewsResult) {
    throw std::runtime_error{"Failed to get image views from swapchain"};
  }

  if (!imagesResult) {
    throw std::runtime_error{"Failed to get imags from swapchain"};
  }

  swapchainImageViews = imageViewsResult.value();
  swapchainImages = imagesResult.value();
}

void VkEngine::createDepthImage() {
  depthImage = Image{allocator, device.device, commandPool, queue, vk::Extent3D{swapchain.extent}.setDepth(1), vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth};
}

void VkEngine::createViewportAndScissors() {
  vk::Extent3D extent = vk::Extent3D{}
    .setDepth(0)
    .setHeight(swapchain.extent.height)
    .setWidth(swapchain.extent.width);

  auto [v, s] = utils::createViewportAndScissors(extent);

  viewport = v;
  scissors = s;
};

void VkEngine::createQueue() {
  vkb::Result<uint32_t> queueIndexResult = device.get_queue_index(vkb::QueueType::graphics);

  if (!queueIndexResult) {
    throw std::runtime_error{"Failed to get a queue index"  + queueIndexResult.error().message()};
  }

  vkb::Result<VkQueue> queueResult = device.get_queue(vkb::QueueType::graphics);

  if (!queueResult) {
    throw std::runtime_error{"Failed to get a queue"  + queueResult.error().message()};
  }

  queue = queueResult.value();
  queueIndex = queueIndexResult.value();
};

void VkEngine::createSyncPrimitives() {
  fences.resize(MAX_CONCURRENT_FRAMES);
  renderCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);
  presentCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);

  vk::Device d = device.device;

  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    fences[i] = d.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
    renderCompleteSemaphores[i] = d.createSemaphore(vk::SemaphoreCreateInfo{});
    presentCompleteSemaphores[i] = d.createSemaphore(vk::SemaphoreCreateInfo{});
  }

  deleteQueue.push_back([&](){
    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
      vk::Device{device}.destroyFence(fences[i]);
      vk::Device{device}.destroySemaphore(renderCompleteSemaphores[i]);
      vk::Device{device}.destroySemaphore(presentCompleteSemaphores[i]);
    }
  });
};

void VkEngine::createCommandPool() {
  vk::CommandPoolCreateInfo commandPoolCreateInfo = vk::CommandPoolCreateInfo{}
    .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
    .setQueueFamilyIndex(queueIndex);

  vk::Device d = device.device;

  commandPool = d.createCommandPool(commandPoolCreateInfo, nullptr);
  deleteQueue.push_back([&]() { vk::Device{device.device}.destroyCommandPool(commandPool); });
};

void VkEngine::createCommandBuffers() {
  commadBuffers.resize(MAX_CONCURRENT_FRAMES);  
  vk::Device d = device.device;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{}
    .setCommandPool(commandPool)
    .setCommandBufferCount(MAX_CONCURRENT_FRAMES)
    .setLevel(vk::CommandBufferLevel::ePrimary);
  
  if (d.allocateCommandBuffers(&commandBufferAllocateInfo, commadBuffers.data()) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };
};


void VkEngine::createPipelines() {
  vk::Device d = vk::Device{device};

  vk::DescriptorSetLayoutBinding projectionBidning = vk::DescriptorSetLayoutBinding{}
    .setBinding(0)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eVertex)
    .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  std::vector<vk::DescriptorSetLayoutBinding> uniformBindings = {projectionBidning};

  vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{}
    .setBindings(uniformBindings)
    .setBindingCount(uniformBindings.size());

  vk::DescriptorSetLayout descriptorSetLayout = d.createDescriptorSetLayout(descriptorSetLayoutCreateInfo, nullptr);

  pipelines.push_back(
    Pipeline{
      allocator,
      Shader{d, "./shaders/default.vert.spv"},
      Shader{d, "./shaders/default-solid.frag.spv"},
      d,
      viewport,
      scissors,
      MAX_CONCURRENT_FRAMES,
      descriptorPool,
      {textureSetLayout, descriptorSetLayout, objectSetLayout},
    }
  );

  pipelines.push_back(
    Pipeline{
      allocator,
      Shader{d, "./shaders/default.vert.spv"},
      Shader{d, "./shaders/default.frag.spv"},
      d,
      viewport,
      scissors,
      MAX_CONCURRENT_FRAMES,
      descriptorPool,
      {textureSetLayout, descriptorSetLayout, objectSetLayout},
    }
  );
};

void VkEngine::createSampler() {
  vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo{}
    .setMagFilter(vk::Filter::eLinear)
    .setMinFilter(vk::Filter::eLinear)
    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
    .setAnisotropyEnable(0)
    .setCompareEnable(0);

  sampler = vk::Device{device}.createSampler(samplerCreateInfo);
}

void VkEngine::createDescriptorPool() {
  vk::DescriptorPoolSize samplerPool = vk::DescriptorPoolSize{}
    .setDescriptorCount(MAX_CONCURRENT_FRAMES * 64)
    .setType(vk::DescriptorType::eCombinedImageSampler);

  vk::DescriptorPoolSize uniformPool = vk::DescriptorPoolSize{}
    .setDescriptorCount(MAX_CONCURRENT_FRAMES * 64)
    .setType(vk::DescriptorType::eUniformBuffer);

  std::vector<vk::DescriptorPoolSize> poolSizes{
    samplerPool,
    uniformPool
  };

  vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{}
    .setMaxSets(MAX_CONCURRENT_FRAMES * 64)
    .setPoolSizes(poolSizes)
    .setPoolSizeCount(poolSizes.size());

  descriptorPool = vk::Device{device}.createDescriptorPool(descriptorPoolCreateInfo, nullptr);

  vk::Device d = vk::Device{device};

  vk::DescriptorSetLayoutBinding samplerBinding = vk::DescriptorSetLayoutBinding{}
    .setBinding(0)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eFragment)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
    .setImmutableSamplers(sampler);

  vk::DescriptorSetLayoutBinding objectBidning = vk::DescriptorSetLayoutBinding{}
    .setBinding(0)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eVertex)
    .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutCreateInfo textureSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{}
    .setBindings(samplerBinding)
    .setBindingCount(1);

  vk::DescriptorSetLayoutCreateInfo objectSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{}
    .setBindings(objectBidning)
    .setBindingCount(1);

  textureSetLayout = d.createDescriptorSetLayout(textureSetLayoutCreateInfo, nullptr);
  objectSetLayout = d.createDescriptorSetLayout(objectSetLayoutCreateInfo, nullptr);
}

void VkEngine::setProjection(const Projection& p) {
  projection = p;
}

void VkEngine::addObject(const UniformBuffer& uniform, const uint32_t textureIdx, const uint32_t meshIdx, const uint32_t pipelineIdx) {
  Object object{
    allocator,
    vk::Device{device},
    MAX_CONCURRENT_FRAMES,
    descriptorPool,
    objectSetLayout,
  };
  
  object.uniform = uniform;
  object.textureIdx = textureIdx;
  object.meshIdx = meshIdx;
  object.pipelineIdx = pipelineIdx;

  objects.push_back(object);
}

void VkEngine::loadMesh(const std::string_view path) {
  meshes.push_back(Mesh{allocator, path});
}

void VkEngine::loadTexture(const std::string_view path) {
  Image image{allocator, vk::Device{device}, commandPool, queue, path, vk::ImageLayout::eShaderReadOnlyOptimal};

  textures.push_back(
    Texture{
      sampler,
      vk::Device{device},
      descriptorPool,
      image,
      MAX_CONCURRENT_FRAMES,
      textureSetLayout,
    }
  );
}
