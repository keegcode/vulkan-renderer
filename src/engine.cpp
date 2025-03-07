#include "engine.hpp"
#include <vulkan/vulkan_core.h>
#include "VkBootstrap.h"
#include "buffer.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "scene.hpp"
#include "utils.hpp"

#include <cstdint>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <string_view>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

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
  createCommandBuffer();
  createSampler();
  createDescriptorSetLayouts();
  createUniformBuffer();
  createDescriptors();
  createDepthImage();
  createViewportAndScissors();
  createPipeline();
}

void Engine::destroySwapchainResources() {
  vk::Device d = device.device;

  for (const vk::ImageView imageView : swapchainImageViews) {
    d.destroyImageView(imageView);
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

  if (d.waitForFences(1, &fence, 1, UINT64_MAX) !=
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
      swap, UINT64_MAX, presentCompleteSemaphore, nullptr, &imageIndex);

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

  if (d.resetFences(1, &fence) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to reset fence"};
  };

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

  std::array<vk::DeviceSize, 1> offsets = {0};

  for (size_t i = 0; i < objects.size(); i++) {
    const Object& object = objects[i];
    const Mesh& mesh = meshes[object.meshIdx];
    const Material& material = materials[object.materialIdx];
    const Texture& texture = textures[object.textureIdx];

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               pipeline.graphicsPipeline);

    projection.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);

    vmaCopyMemoryToAllocation(allocator, &projection,
                              uniformBuffer.allocation, 0,
                              sizeof(Projection));

    vmaCopyMemoryToAllocation(allocator, &light,
                              uniformBuffer.allocation, sizeof(Projection),
                              sizeof(Light));

    vmaCopyMemoryToAllocation(allocator, &material,
                              uniformBuffer.allocation, sizeof(Projection) + sizeof(Light) + sizeof(Material) * object.materialIdx,
                              sizeof(Material));

    vmaCopyMemoryToAllocation(allocator, &material,
                              uniformBuffer.allocation, sizeof(Projection) + sizeof(Light) + sizeof(Material) * object.materialIdx + sizeof(Object) * i,
                              sizeof(Object));

    vmaCopyMemoryToAllocation(allocator, &light,
                              uniformBuffer.allocation, sizeof(Projection),
                              sizeof(Light));

    commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer.buffer,
                                    offsets.data());

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
          .setWaitSemaphores(presentCompleteSemaphore)
          .setCommandBuffers(commandBuffer)
          .setCommandBufferCount(1)
          .setSignalSemaphores(renderCompleteSemaphore)
          .setSignalSemaphoreCount(1)
          .setWaitDstStageMask(waitStage);

  vk::Result queueSubmitResult = queue.submit(1, &submitInfo, fence);

  if (queueSubmitResult != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to submit to queue"};
  };

  uint32_t imageIndices = {imageIndex};

  vk::PresentInfoKHR presentInfo =
      vk::PresentInfoKHR{}
          .setWaitSemaphoreCount(1)
          .setWaitSemaphores(renderCompleteSemaphore)
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

  d.destroyFence(fence);
  d.destroySemaphore(renderCompleteSemaphore);
  d.destroySemaphore(presentCompleteSemaphore);

  for (Texture& texture : textures) {
    texture.destroy(allocator, d);
  }

  for (Mesh& mesh : meshes) {
    mesh.destroy(allocator);
  }

  pipeline.destroy(d);

  destroySwapchainResources();

  d.destroySampler(sampler);
  d.destroyCommandPool(commandPool);

  d.destroyDescriptorSetLayout(uniformLayout);
  d.destroyDescriptorSetLayout(imageSamplerLayout);

  uniformBuffer.destroy(allocator);

  uniformDescriptor.destroy(allocator);
  imageSamplerDescriptor.destroy(allocator);

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

  vk::PhysicalDevice{physicalDevice.physical_device}
    .getProperties2(&physicalDeviceProperties);
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
  int32_t w, h;
  SDL_GetWindowSize(display.window, &w, &h);

  vk::Extent2D extent = vk::Extent2D{}.setWidth(w).setHeight(h);

  swapchain =
      utils::createSwapchain(device, extent, 1, &swapchain);

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
  vk::Device d = device.device;

  fence = d.createFence(
      vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
  renderCompleteSemaphore = d.createSemaphore(vk::SemaphoreCreateInfo{});
  presentCompleteSemaphore = d.createSemaphore(vk::SemaphoreCreateInfo{});
};

void Engine::createCommandPool() {
  vk::CommandPoolCreateInfo commandPoolCreateInfo =
      vk::CommandPoolCreateInfo{}
          .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
          .setQueueFamilyIndex(queueIndex);

  vk::Device d = device.device;

  commandPool = d.createCommandPool(commandPoolCreateInfo, nullptr);
};

void Engine::createCommandBuffer() {
  vk::Device d = device.device;

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount(1)
          .setLevel(vk::CommandBufferLevel::ePrimary);

  if (d.allocateCommandBuffers(&commandBufferAllocateInfo, &commandBuffer) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };
};

void Engine::createPipeline() {
  vk::Device d = vk::Device{device};

  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{
    imageSamplerLayout, 
    uniformLayout,
    uniformLayout,
    uniformLayout,
    uniformLayout,
  };

  pipeline = Pipeline{Shader{d, "./shaders/shader.vert.glsl.spv"},
                      Shader{d, "./shaders/shader.frag.glsl.spv"},
                      vk::Device{device},
                      viewport,
                      scissors,
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

  vk::DescriptorSetLayoutBinding imageSamplerBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setImmutableSamplers(sampler);

  vk::DescriptorSetLayoutBinding uniformBinding =
      vk::DescriptorSetLayoutBinding{}
          .setBinding(0)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  vk::DescriptorSetLayoutCreateInfo imageSamplerSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(imageSamplerBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  vk::DescriptorSetLayoutCreateInfo uniformSetLayoutCreateInfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setBindings(uniformBinding)
          .setBindingCount(1)
          .setFlags(
              vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);

  imageSamplerLayout =
      d.createDescriptorSetLayout(imageSamplerSetLayoutCreateInfo, nullptr);

  uniformLayout =
      d.createDescriptorSetLayout(uniformSetLayoutCreateInfo, nullptr);
}

void Engine::createUniformBuffer() {
  uniformBuffer = Buffer{
    allocator, 
    sizeof(Projection) + sizeof(Light) + sizeof(Material) * materials.size() + sizeof(Object) * objects.size(),
    vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress};
}

void Engine::setLight(const Light& l) {
  Object object{};
  object.matrix = glm::scale(glm::translate(object.matrix, light.pos), glm::vec3{0.5f});
  object.color = glm::vec3{1.0};
  
  objects.push_back(object);

  light = l;
}

void Engine::loadMesh(const std::string_view path) {
  meshes.push_back(Mesh{allocator, path});
}

void Engine::loadTexture(const std::string_view path) {
  Image image{allocator,   vk::Device{device},
              commandPool, queue,
              path,        vk::ImageLayout::eShaderReadOnlyOptimal};

  Texture texture{image};

  textures.push_back(texture);
}

void Engine::createDescriptors() {
  vk::Device d{device};

  uniformDescriptor.type = vk::DescriptorType::eUniformBuffer;
  imageSamplerDescriptor.type = vk::DescriptorType::eCombinedImageSampler;

  uniformDescriptor.layoutSize = utils::getAlignedSize(
    d.getDescriptorSetLayoutSizeEXT(uniformLayout, dld),
    descriptorBufferProperties.descriptorBufferOffsetAlignment
  );

  imageSamplerDescriptor.layoutSize = utils::getAlignedSize(
    d.getDescriptorSetLayoutSizeEXT(imageSamplerLayout, dld),
    descriptorBufferProperties.descriptorBufferOffsetAlignment
  );
  
  uniformDescriptor.offset = d.getDescriptorSetLayoutBindingOffsetEXT(uniformLayout, 0, dld);
  imageSamplerDescriptor.offset = d.getDescriptorSetLayoutBindingOffsetEXT(imageSamplerLayout, 0, dld);

  uniformDescriptor.buffer = Buffer{
    allocator, 
    uniformDescriptor.layoutSize * (2 + objects.size() + materials.size()),
    vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress,
    VMA_MEMORY_USAGE_CPU_TO_GPU,
    VMA_ALLOCATION_CREATE_MAPPED_BIT
  };

  imageSamplerDescriptor.buffer = Buffer{
    allocator, 
    imageSamplerDescriptor.layoutSize * textures.size(),
    vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT | vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress,
    VMA_MEMORY_USAGE_CPU_TO_GPU,
    VMA_ALLOCATION_CREATE_MAPPED_BIT
  };

  uniformDescriptor.address.setDeviceAddress(uniformDescriptor.buffer.getDeviceAddress(d));
  imageSamplerDescriptor.address.setDeviceAddress(imageSamplerDescriptor.buffer.getDeviceAddress(d));

  uint8_t* imageSamplerDescriptorPtr = reinterpret_cast<uint8_t*>(imageSamplerDescriptor.buffer.allocationInfo.pMappedData);
  for (size_t i = 0; i < textures.size(); i++) {
  }

  uint8_t* uniformDescriptorPtr = reinterpret_cast<uint8_t*>(uniformDescriptor.buffer.allocationInfo.pMappedData);

  for (size_t i = 0; i < materials.size(); i++) {
  }

  for (size_t i = 0; i < objects.size(); i++) {
  }
}
