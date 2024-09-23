#include "vk-engine.hpp"
#include "VkBootstrap.h"
#include "vk-structs.hpp"
#include "vk-utils.hpp"

#include <SDL_video.h>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>  
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

void VkEngine::init(const Display& d, const Transform& transform, const std::string_view meshPath, const std::string_view texturePath) {
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
  createMesh(meshPath);
  createShaders(transform, texturePath);
  createDepthImage();
  createViewportAndScissors();
  createGraphicsPipeline();
};
  
void VkEngine::destroySwapchainResources() {
  vk::Device d = device.device;

  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    d.destroyImageView(swapchainImageViews[i]);
  }

  d.destroyImageView(depthImage.imageView);
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

void VkEngine::drawFrame() {
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
  vk::DescriptorSet descriptorSet = fragmentShader.descriptorSets[frame];

  commandBuffer.reset();

  vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}
    .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  commandBuffer.begin(beginInfo);

  vk::ClearValue clearValue = vk::ClearValue{}.setColor(vk::ClearColorValue{}.setUint32({0xFF, 0XFF, 0xFF, 0xFF}));
  vk::ClearValue depthClearValue = vk::ClearValue{}.setDepthStencil(vk::ClearDepthStencilValue{}.setDepth(1.0f).setStencil(0));

  vk::RenderingAttachmentInfo depthAttachment = vk::RenderingAttachmentInfo{}
    .setImageView(depthImage.imageView)
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

  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
  
  std::vector<vk::Buffer> buffers{};
  vk::DeviceSize offsets[1] = {0};

  for (const Mesh& mesh : meshes) {
    buffers.push_back(mesh.vertexBuffer.buffer);
  }

  commandBuffer.bindVertexBuffers(0, meshes.size(), buffers.data(), offsets);

  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

  for (const Entity& entity : entities) {
    commandBuffer.bindIndexBuffer(meshes[entity.meshIndex].indexBuffer.buffer, 0, vk::IndexType::eUint16);
    commandBuffer.drawIndexed(meshes[entity.meshIndex].indices.size(), 1, 0, 0, 1);
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

void VkEngine::processInput() {
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
  depthImage = utils::createImage(allocator, device.device, commandPool, queue, vk::Extent3D{swapchain.extent}.setDepth(1), vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth);
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

void VkEngine::createShaders(const Transform& transform, const std::string_view& texturePath) {
  vertexShader = VertexShader{};
  fragmentShader = FragmentShader{};
  
  vertexShader.init(device.device, "./shaders/vert.spv");

  Buffer ubo = utils::createBuffer(allocator, &transform, sizeof(Transform), vk::BufferUsageFlagBits::eUniformBuffer);
  Image texture = utils::createTextureImage(allocator, device.device, commandPool, queue, texturePath, vk::ImageLayout::eShaderReadOnlyOptimal);

  fragmentShader.init(device.device, "./shaders/frag.spv", MAX_CONCURRENT_FRAMES, ubo, texture);
  
  deleteQueue.push_back([=]() {
    vertexShader.destroy();
    fragmentShader.destroy();
    vk::Device{device}.destroyImageView(texture.imageView);
    vmaDestroyImage(allocator, texture.image, texture.allocation);
    vmaDestroyBuffer(allocator, ubo.buffer, ubo.allocation);
  });
}

void VkEngine::createMesh(const std::string_view path) {
  Assimp::Importer importer{};

  const aiScene* scene = importer.ReadFile(
    path.data(), 
    aiProcess_Triangulate | 
    aiProcess_FlipUVs |
    aiProcess_GenNormals |
    aiProcess_GenUVCoords
  );

  if (!scene) {
    throw std::runtime_error{std::string{"Failed to read mesh file: "} + importer.GetErrorString()};
  }

  Mesh mesh{};

  for (size_t i = 0; i < scene->mNumMeshes; i++) {
    aiMesh* assimpMesh = scene->mMeshes[i];

    for (size_t j = 0; j < assimpMesh->mNumVertices; j++) {
      Vertex vertex{};

      vertex.pos[0] = assimpMesh->mVertices[j].x;
      vertex.pos[1] = assimpMesh->mVertices[j].y;
      vertex.pos[2] = assimpMesh->mVertices[j].z;

      vertex.clr[0] = 255.0f / (uint8_t) rand(); 
      vertex.clr[1] = 255.0f / (uint8_t) rand(); 
      vertex.clr[2] = 255.0f / (uint8_t) rand(); 

      if (assimpMesh->HasTextureCoords(0)) {
        vertex.uv[0] = assimpMesh->mTextureCoords[0][j].x;
        vertex.uv[1] = assimpMesh->mTextureCoords[0][j].y;
      }

      mesh.vertices.push_back(vertex);
    }

    for (size_t j = 0; j < assimpMesh->mNumFaces; j++) {
      assert(assimpMesh->mFaces[j].mNumIndices == 3);
      mesh.indices.push_back(assimpMesh->mFaces[j].mIndices[0]);
      mesh.indices.push_back(assimpMesh->mFaces[j].mIndices[1]);
      mesh.indices.push_back(assimpMesh->mFaces[j].mIndices[2]);
    }
  }

  importer.FreeScene();
  
  Buffer vertexBuffer = utils::createBuffer(allocator, mesh.vertices.data(), sizeof(Vertex) * mesh.vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer);
  Buffer indexBuffer = utils::createBuffer(allocator, mesh.indices.data(), sizeof(uint16_t) * mesh.indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);

  mesh.vertexBuffer = vertexBuffer;
  mesh.indexBuffer = indexBuffer;

  meshes.push_back(mesh);

  deleteQueue.push_back([=]() {
    vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
    vmaDestroyBuffer(allocator, mesh.indexBuffer.buffer, mesh.indexBuffer.allocation);
  });
}

void VkEngine::createGraphicsPipeline() {
  vk::Device d = device.device;

  vk::VertexInputBindingDescription vertexInputBindingDescription = vk::VertexInputBindingDescription{}
    .setStride(sizeof(Vertex))
    .setInputRate(vk::VertexInputRate::eVertex)
    .setBinding(0);

  vk::VertexInputAttributeDescription vertexPositionAttributeDescription = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(0)
    .setOffset(offsetof(Vertex, pos))
    .setFormat(vk::Format::eR32G32B32Sfloat);

  vk::VertexInputAttributeDescription vertexColorAttributeDescription = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(1)
    .setOffset(offsetof(Vertex, clr))
    .setFormat(vk::Format::eR32G32B32Sfloat);

  vk::VertexInputAttributeDescription vertexTextureCoordAttributeDescription = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(2)
    .setOffset(offsetof(Vertex, uv))
    .setFormat(vk::Format::eR32G32Sfloat);

  vk::PipelineShaderStageCreateInfo vertexShaderStage = vk::PipelineShaderStageCreateInfo{}
    .setStage(vk::ShaderStageFlagBits::eVertex)
    .setModule(vertexShader.module)
    .setPName("main")
    .setPSpecializationInfo(nullptr);

  vk::PipelineShaderStageCreateInfo fragmentShaderStage = vk::PipelineShaderStageCreateInfo{}
    .setStage(vk::ShaderStageFlagBits::eFragment)
    .setModule(fragmentShader.module)
    .setPName("main")
    .setPSpecializationInfo(nullptr);
  
  vk::Format colorAttachmentFormat = vk::Format::eB8G8R8A8Srgb;
  vk::Format depthAttachmentFormat = vk::Format::eD32Sfloat;

  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{}
    .setColorAttachmentCount(1)
    .setDepthAttachmentFormat(depthAttachmentFormat)
    .setColorAttachmentFormats(colorAttachmentFormat);
  
  std::vector<vk::PipelineShaderStageCreateInfo> stages{
    vertexShaderStage,
    fragmentShaderStage,
  };

  std::vector<vk::VertexInputAttributeDescription> attributes{
    vertexPositionAttributeDescription, 
    vertexColorAttributeDescription, 
    vertexTextureCoordAttributeDescription 
  };
  
  vk::PipelineVertexInputStateCreateInfo vertexInputState = vk::PipelineVertexInputStateCreateInfo{}
    .setVertexBindingDescriptions(vertexInputBindingDescription)
    .setVertexBindingDescriptionCount(1)
    .setVertexAttributeDescriptionCount(attributes.size())
    .setVertexAttributeDescriptions(attributes);

  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState = vk::PipelineInputAssemblyStateCreateInfo{}
    .setTopology(vk::PrimitiveTopology::eTriangleList)
    .setPrimitiveRestartEnable(0);

  vk::PipelineViewportStateCreateInfo viewportState = vk::PipelineViewportStateCreateInfo{}
    .setScissors(scissors)
    .setViewports(viewport)
    .setViewportCount(1)
    .setScissorCount(1);

  vk::PipelineRasterizationStateCreateInfo rasterizationState = vk::PipelineRasterizationStateCreateInfo{}
    .setRasterizerDiscardEnable(0)
    .setDepthClampEnable(0)
    .setDepthBiasEnable(0)
    .setPolygonMode(vk::PolygonMode::eFill)
    .setCullMode(vk::CullModeFlagBits::eBack)
    .setFrontFace(vk::FrontFace::eCounterClockwise)
    .setLineWidth(1.0f);

  vk::PipelineMultisampleStateCreateInfo multisampleState = vk::PipelineMultisampleStateCreateInfo{}
    .setRasterizationSamples(vk::SampleCountFlagBits::e1)
    .setSampleShadingEnable(0);

  vk::PipelineDepthStencilStateCreateInfo depthStencilState = vk::PipelineDepthStencilStateCreateInfo{}
    .setDepthTestEnable(1)
    .setDepthWriteEnable(1)
    .setStencilTestEnable(0)
    .setDepthBoundsTestEnable(0)
    .setDepthCompareOp(vk::CompareOp::eLessOrEqual);

  vk::PipelineColorBlendAttachmentState colorBlendAttachmentState = vk::PipelineColorBlendAttachmentState{}
      .setBlendEnable(0)
      .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
  
  vk::PipelineColorBlendStateCreateInfo colorBlendState = vk::PipelineColorBlendStateCreateInfo{}
    .setAttachmentCount(1)
    .setAttachments(colorBlendAttachmentState);
  
  vk::DynamicState dynamicStates[2] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamicState = vk::PipelineDynamicStateCreateInfo{}
    .setDynamicStates(dynamicStates)
    .setDynamicStateCount(2);

  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo{}
      .setSetLayouts(fragmentShader.descriptorSetLayout)
      .setSetLayoutCount(1);

  pipelineLayout = d.createPipelineLayout(pipelineLayoutCreateInfo, nullptr);
  deleteQueue.push_back([&]() { vk::Device{device.device}.destroyPipelineLayout(pipelineLayout); });
    
  vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo = vk::GraphicsPipelineCreateInfo{}
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
    .setLayout(pipelineLayout);

  vk::ResultValue<vk::Pipeline> pipelineResult = d.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);

  if (pipelineResult.result != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to create a pipeline"};
  }

  graphicsPipeline = pipelineResult.value;
  deleteQueue.push_back([&]() { vk::Device{device.device}.destroyPipeline(graphicsPipeline); });
};
