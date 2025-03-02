#include "engine.hpp"
#include "VkBootstrap.h"
#include "buffer.hpp"
#include "descriptor.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "scene.hpp"
#include "utils.hpp"

#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

void Engine::init(const Display& d) {
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
  createDescriptorSetLayouts();
  createDepthImage();
  createViewportAndScissors();
  createPipeline();
}

void Engine::destroySwapchainResources() {
  vk::Device d = device.device;

  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    d.destroyImageView(swapchainImageViews[i]);
  }

  d.destroyImageView(depthImage.view);
  vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
}

void Engine::rebuiltSwapchain() {
  vk::Device d = device.device;
  vkb::Swapchain old = swapchain;

  d.waitIdle();

  destroySwapchainResources();

  createSwapchain();
  createDepthImage();
  createViewportAndScissors();

  vkb::destroy_swapchain(old);
}

void Engine::drawFrame(float deltaTime) {
  vk::Device d = device.device;

  if (d.waitForFences(1, &fences[frame], 1, UINT64_MAX) !=
      vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to wait for fence"};
  };

  if (shouldBeResized) {
    rebuiltSwapchain();
    shouldBeResized = false;
  }

  vk::SwapchainKHR swap = swapchain.swapchain;

  uint32_t imageIndex;
  vk::Result acquireResult = d.acquireNextImageKHR(
      swap, UINT64_MAX, presentCompleteSemaphores[frame], nullptr, &imageIndex);

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

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}.setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);

  vk::ClearValue clearValue = vk::ClearValue{}.setColor(
      vk::ClearColorValue{}.setUint32({0xFF, 0XFF, 0xFF, 0xFF}));
  vk::ClearValue depthClearValue = vk::ClearValue{}.setDepthStencil(
      vk::ClearDepthStencilValue{}.setDepth(1.0f).setStencil(0));

  vk::RenderingAttachmentInfo depthAttachment =
      vk::RenderingAttachmentInfo{}
          .setImageView(depthImage.view)
          .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eNone)
          .setClearValue(depthClearValue);

  vk::RenderingAttachmentInfo attachment =
      vk::RenderingAttachmentInfo{}
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

  commandBuffer.setViewport(0, 1, &viewport);
  commandBuffer.setScissor(0, 1, &scissors);

  vk::DeviceSize offsets[1] = {0};

  for (Object& object : objects) {
    const Mesh& mesh = meshes[object.meshIdx];
    const Texture& texture = textures[object.textureIdx];
    const Material& material = materials[object.materialIdx];

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               pipeline.graphicsPipeline);

    projection.properties.view =
        glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);

    vmaCopyMemoryToAllocation(allocator, &projection,
                              projection.buffer.allocation, 0,
                              sizeof(ProjectionProperties));

    vmaCopyMemoryToAllocation(allocator, &object.properties,
                              object.buffer.allocation, 0,
                              sizeof(ObjectProperties));

    vmaCopyMemoryToAllocation(allocator, &light.properties,
                              light.buffer.allocation, 0,
                              sizeof(LightProperties));

    vmaCopyMemoryToAllocation(allocator, &material.properties,
                              material.buffer.allocation, 0, sizeof(Material));

    std::array<vk::DescriptorBufferBindingInfoEXT, 5> bindingInfo{};

    bindingInfo[0] =
        vk::DescriptorBufferBindingInfoEXT{}
            .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                      vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT)
            .setAddress(texture.descriptor.address.deviceAddress);

    bindingInfo[1] =
        vk::DescriptorBufferBindingInfoEXT{}
            .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
            .setAddress(projection.descriptor.address.deviceAddress);

    bindingInfo[2] =
        vk::DescriptorBufferBindingInfoEXT{}
            .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
            .setAddress(object.descriptor.address.deviceAddress);

    bindingInfo[3] =
        vk::DescriptorBufferBindingInfoEXT{}
            .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
            .setAddress(light.descriptor.address.deviceAddress);

    bindingInfo[4] =
        vk::DescriptorBufferBindingInfoEXT{}
            .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT)
            .setAddress(material.descriptor.address.deviceAddress);

    commandBuffer.bindDescriptorBuffersEXT(bindingInfo, dld);

    std::array<const uint32_t, 5> bufferIndices = {0, 1, 2, 3, 4};

    std::array<const vk::DeviceSize, 5> bufferOffset = {
        texture.descriptor.offset, projection.descriptor.offset,
        object.descriptor.offset, light.descriptor.offset,
        material.descriptor.offset};

    commandBuffer.setDescriptorBufferOffsetsEXT(
        vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 5,
        bufferIndices.data(), bufferOffset.data(), dld);

    commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer, offsets);
    commandBuffer.bindIndexBuffer(mesh.indexBuffer.buffer, 0,
                                  vk::IndexType::eUint16);
    commandBuffer.drawIndexed(mesh.indicesCount, 1, 0, 0, 1);
  }

  commandBuffer.endRendering();

  commandBuffer.end();

  vk::Flags<vk::PipelineStageFlagBits> waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;

  vk::SubmitInfo submitInfo =
      vk::SubmitInfo{}
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

  vk::PresentInfoKHR presentInfo =
      vk::PresentInfoKHR{}
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

void Engine::processInput(float deltaTime) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
      isRunning = false;
      break;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE) {
      isRunning = false;
      break;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_RESIZED) {
      shouldBeResized = true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_RIGHT) {
      SDL_SetRelativeMouseMode(SDL_TRUE);
      SDL_SetWindowGrab(display.window, SDL_TRUE);
      camera.mode = CameraMode::Move;
    }
    if (event.type == SDL_MOUSEBUTTONUP &&
        event.button.button == SDL_BUTTON_RIGHT) {
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

      front.x = glm::cos(glm::radians(camera.yaw)) *
                glm::cos(glm::radians(camera.pitch));
      front.y = glm::sin(glm::radians(camera.pitch));
      front.z = glm::sin(glm::radians(camera.yaw)) *
                glm::cos(glm::radians(camera.pitch));

      camera.front = glm::normalize(front);
      camera.right = glm::normalize(glm::cross(camera.front, camera.up));
    }
  }
};

void Engine::destroy() {
  vk::Device d = device.device;

  d.waitIdle();

  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    d.destroyFence(fences[i]);
    d.destroySemaphore(renderCompleteSemaphores[i]);
    d.destroySemaphore(presentCompleteSemaphores[i]);
  }

  for (Object& object : objects) {
    object.destroy(allocator);
  }

  for (Texture& texture : textures) {
    texture.destroy(allocator, d);
  }

  for (Mesh& mesh : meshes) {
    mesh.destroy(allocator);
  }

  for (Material& material : materials) {
    material.destroy(allocator);
  }

  pipeline.destroy(d);

  light.destroy(allocator);

  destroySwapchainResources();

  d.destroySampler(sampler);
  d.destroyCommandPool(commandPool);

  d.destroyDescriptorSetLayout(objectSetLayout);
  d.destroyDescriptorSetLayout(textureSetLayout);
  d.destroyDescriptorSetLayout(projectionSetLayout);
  d.destroyDescriptorSetLayout(lightSetLayout);
  d.destroyDescriptorSetLayout(materialSetLayout);

  vkb::destroy_swapchain(swapchain);
  vkb::destroy_surface(instance, surface);

  vmaDestroyAllocator(allocator);

  vkb::destroy_device(device);
  vkb::destroy_instance(instance);
};

void Engine::createInstance() {
  vkb::Result<vkb::Instance> instanceResult =
      vkb::InstanceBuilder{}
          .set_app_name("VkRenderer")
          .require_api_version(1, 3)
          .enable_extensions(display.vulkanExtensions)
          .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
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

void Engine::pickPhysicalDevice() {
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

  vk::PhysicalDevice pd{physicalDevice.physical_device};

  pd.getProperties2(&physicalDeviceProperties);
};

void Engine::pickDevice() {
  vkb::Result<vkb::Device> deviceResult =
      vkb::DeviceBuilder{physicalDevice}.build();

  if (!deviceResult) {
    throw std::runtime_error{"Failed to create a device: " +
                             deviceResult.error().message()};
  }

  device = deviceResult.value();
};

void Engine::createAllocator() {
  VmaVulkanFunctions vulkanFunctions{};
  vulkanFunctions.vkGetDeviceProcAddr = instance.fp_vkGetDeviceProcAddr;
  vulkanFunctions.vkGetInstanceProcAddr = instance.fp_vkGetInstanceProcAddr;

  VmaAllocatorCreateInfo createInfo{};
  createInfo.pVulkanFunctions = &vulkanFunctions;
  createInfo.instance = instance.instance;
  createInfo.physicalDevice = physicalDevice.physical_device;
  createInfo.device = device.device;
  createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create a VmaAllocator"};
  };
};

void Engine::createSwapchain() {
  int w, h;
  SDL_GetWindowSize(display.window, &w, &h);

  vk::Extent2D extent = vk::Extent2D{}.setWidth(w).setHeight(h);

  swapchain =
      utils::createSwapchain(device, extent, MAX_CONCURRENT_FRAMES, &swapchain);

  vkb::Result<std::vector<VkImageView>> imageViewsResult =
      swapchain.get_image_views();
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

void Engine::createDepthImage() {
  depthImage = Image{allocator,
                     device.device,
                     vk::Extent3D{swapchain.extent}.setDepth(1),
                     vk::Format::eD32Sfloat,
                     vk::ImageUsageFlagBits::eDepthStencilAttachment,
                     vk::ImageAspectFlagBits::eDepth};
}

void Engine::createViewportAndScissors() {
  vk::Extent3D extent = vk::Extent3D{}
                            .setDepth(0)
                            .setHeight(swapchain.extent.height)
                            .setWidth(swapchain.extent.width);

  auto [v, s] = utils::createViewportAndScissors(extent);

  viewport = v;
  scissors = s;
};

void Engine::createQueue() {
  vkb::Result<uint32_t> queueIndexResult =
      device.get_queue_index(vkb::QueueType::graphics);

  if (!queueIndexResult) {
    throw std::runtime_error{"Failed to get a queue index" +
                             queueIndexResult.error().message()};
  }

  vkb::Result<VkQueue> queueResult = device.get_queue(vkb::QueueType::graphics);

  if (!queueResult) {
    throw std::runtime_error{"Failed to get a queue" +
                             queueResult.error().message()};
  }

  queue = queueResult.value();
  queueIndex = queueIndexResult.value();
};

void Engine::createSyncPrimitives() {
  fences.resize(MAX_CONCURRENT_FRAMES);
  renderCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);
  presentCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);

  vk::Device d = device.device;

  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    fences[i] = d.createFence(
        vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
    renderCompleteSemaphores[i] = d.createSemaphore(vk::SemaphoreCreateInfo{});
    presentCompleteSemaphores[i] = d.createSemaphore(vk::SemaphoreCreateInfo{});
  }
};

void Engine::createCommandPool() {
  vk::CommandPoolCreateInfo commandPoolCreateInfo =
      vk::CommandPoolCreateInfo{}
          .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
          .setQueueFamilyIndex(queueIndex);

  vk::Device d = device.device;

  commandPool = d.createCommandPool(commandPoolCreateInfo, nullptr);
};

void Engine::createCommandBuffers() {
  commadBuffers.resize(MAX_CONCURRENT_FRAMES);
  vk::Device d = device.device;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount(MAX_CONCURRENT_FRAMES)
          .setLevel(vk::CommandBufferLevel::ePrimary);

  if (d.allocateCommandBuffers(&commandBufferAllocateInfo,
                               commadBuffers.data()) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };
};

void Engine::createPipeline() {
  vk::Device d = vk::Device{device};

  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{
      textureSetLayout, projectionSetLayout, objectSetLayout, lightSetLayout,
      materialSetLayout};

  pipeline = Pipeline{Shader{d, "./shaders/shader.vert.glsl.spv"},
                      Shader{d, "./shaders/shader.frag.glsl.spv"},
                      vk::Device{device},
                      viewport,
                      scissors,
                      MAX_CONCURRENT_FRAMES,
                      descriptorSetLayouts};
};

void Engine::createSampler() {
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

  sampler = vk::Device{device}.createSampler(samplerCreateInfo);
}

void Engine::createDescriptorSetLayouts() {
  vk::Device d = vk::Device{device};

  vk::DescriptorSetLayoutBinding samplerBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setImmutableSamplers(sampler);

  vk::DescriptorSetLayoutBinding projectionBidning =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutBinding objectBidning =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutBinding lightBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutBinding materialBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutCreateInfo textureSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(samplerBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo objectSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(objectBidning)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo lightSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(lightBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo projectionSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(projectionBidning)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo materialSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(materialBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  textureSetLayout =
      d.createDescriptorSetLayout(textureSetLayoutCreateInfo, nullptr);
  projectionSetLayout =
      d.createDescriptorSetLayout(projectionSetLayoutCreateInfo, nullptr);
  objectSetLayout =
      d.createDescriptorSetLayout(objectSetLayoutCreateInfo, nullptr);
  lightSetLayout =
      d.createDescriptorSetLayout(lightSetLayoutCreateInfo, nullptr);
  materialSetLayout =
      d.createDescriptorSetLayout(materialSetLayoutCreateInfo, nullptr);
}

void Engine::setProjection(const ProjectionProperties& properties) {
  projection = Projection{
      properties,
      Descriptor{dld, descriptorBufferProperties, vk::Device{device.device},
                 projectionSetLayout, vk::DescriptorType::eUniformBuffer,
                 allocator,
                 vk::BufferUsageFlagBits::eUniformBuffer |
                     vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress},
      Buffer::createUniformBuffer(allocator, sizeof(ProjectionProperties))};
}

void Engine::setLight(const LightProperties& properties) {
  Light l{
      properties,
      Descriptor{dld, descriptorBufferProperties, vk::Device{device.device},
                 lightSetLayout, vk::DescriptorType::eUniformBuffer, allocator,
                 vk::BufferUsageFlagBits::eUniformBuffer |
                     vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress},
      Buffer::createUniformBuffer(allocator, sizeof(LightProperties))};

  ObjectProperties object{};

  object.translation = glm::translate(object.translation, properties.pos);
  object.color = glm::vec3{1.0};
  object.scale = glm::scale(object.scale, glm::vec3{0.5});

  addObject(object, 0, 0, 0);

  light = l;

  light.descriptor.setUniformBuffer(dld, vk::Device{device.device},
                                    descriptorBufferProperties, light.buffer);
}

void Engine::addObject(const ObjectProperties& properties,
                       const uint32_t meshIdx,
                       const uint32_t textureIdx,
                       const uint32_t materialIdx) {
  Object object{
      properties,
      Descriptor{dld, descriptorBufferProperties, vk::Device{device.device},
                 objectSetLayout, vk::DescriptorType::eUniformBuffer, allocator,
                 vk::BufferUsageFlagBits::eUniformBuffer |
                     vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress},
      Buffer::createUniformBuffer(allocator, sizeof(ObjectProperties))};

  object.textureIdx = textureIdx;
  object.meshIdx = meshIdx;
  object.materialIdx = materialIdx;

  object.descriptor.setUniformBuffer(dld, vk::Device{device.device},
                                     descriptorBufferProperties, object.buffer);

  objects.push_back(object);
}

void Engine::loadMesh(const std::string_view path) {
  meshes.push_back(Mesh{allocator, path});
}

void Engine::addMaterial(const MaterialProperties& properties) {
  Material material{
      properties,

      Descriptor{dld, descriptorBufferProperties, vk::Device{device.device},
                 materialSetLayout, vk::DescriptorType::eUniformBuffer,
                 allocator,
                 vk::BufferUsageFlagBits::eUniformBuffer |
                     vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress},
      Buffer::createUniformBuffer(allocator, sizeof(MaterialProperties))};

  material.descriptor.setUniformBuffer(dld, vk::Device{device.device},
                                       descriptorBufferProperties,
                                       material.buffer);

  materials.push_back(material);
}

void Engine::loadTexture(const std::string_view path) {
  Image image{allocator,   vk::Device{device},
              commandPool, queue,
              path,        vk::ImageLayout::eShaderReadOnlyOptimal};

  Descriptor descriptor{
      dld,
      descriptorBufferProperties,
      vk::Device{device.device},
      textureSetLayout,
      vk::DescriptorType::eCombinedImageSampler,
      allocator,
      vk::BufferUsageFlagBits::eUniformBuffer |
          vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
          vk::BufferUsageFlagBits::eShaderDeviceAddress |
          vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT};

  Texture texture{image, descriptor, sampler};

  descriptor.setCombinedImageSampler(dld, vk::Device{device.device},
                                     descriptorBufferProperties, image.view,
                                     image.layout, texture.sampler);

  textures.push_back(texture);
}
