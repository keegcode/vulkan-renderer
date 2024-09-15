#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Vertex {
  float pos[3];
  float clr[3];
  float uv[2];
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

std::vector<char> readFile(const std::string_view& path);

struct Display {
  SDL_Window* window;
  SDL_DisplayMode displayMode;
  std::vector<const char*> vulkanExtensions;
};

Display initDisplay();
void destroyDisplay(const Display& display);
VkSurfaceKHR createVulkanSurface(const Display& display);

struct Texture {
  int w;
  int h;
  vk::Image image;
  vk::ImageView imageView;
  VmaAllocation allocation;
};

struct Projection {
  alignas(16) glm::mat4 world;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct Buffer {
  vk::Buffer buffer;
  VmaAllocation allocation;
};

struct VulkanEngine {
  Display display;
  vkb::Instance instance;
  vk::SurfaceKHR surface;
  vkb::Device device;
  vkb::PhysicalDevice physicalDevice;
  vkb::Swapchain swapchain;
  VmaAllocator allocator;
  std::deque<std::function<void()>> deleteQueue;
  std::vector<vk::Fence> fences;
  std::vector<vk::Semaphore> renderCompleteSemaphores;
  std::vector<vk::Semaphore> presentCompleteSemaphores;
  uint8_t MAX_CONCURRENT_FRAMES;
  vk::Queue queue;
  uint32_t queueIndex;
  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commadBuffers;
  vk::DescriptorPool descriptorPool;
  vk::Sampler sampler;
  vk::DescriptorSetLayout descriptorSetLayout;
  std::vector<vk::DescriptorSet> descriptorSets;
  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
  Buffer vertexBuffer;
  Buffer indexBuffer;
  Buffer uniformBuffer;
  Texture texture;
  vk::ShaderModule vertexShader;
  vk::ShaderModule fragmentShader;
  vk::Rect2D scissors;
  vk::Viewport viewport;
  vk::Pipeline graphicsPipeline;
  vk::PipelineLayout pipelineLayout;
  vk::ClearValue clearValue;
  uint16_t frame;
};

VulkanEngine initEngine(const Display& display);

void createInstance(VulkanEngine& engine);
void pickDevices(VulkanEngine& engine);
void createAllocator(VulkanEngine& engine);
void createSwapchain(VulkanEngine& engine);
void createSyncPrimitives(VulkanEngine& engine);
void createQueues(VulkanEngine& engine);
void createImages(VulkanEngine& engine);
void createViewportAndScissors(VulkanEngine& engine);
void createVertexAndIndexBuffers(VulkanEngine& engine);
void createShaderModules(VulkanEngine& engine);
void createDescriptors(VulkanEngine& engine);
void createCommandBuffers(VulkanEngine& engine);
void createGraphicsPipeline(VulkanEngine& engine);
void createTexture(const char* path, VulkanEngine& engine);
void createUniformBuffer(VulkanEngine& engine, Projection& projection);

vk::CommandBuffer beginSingleSubmitCommand(vk::Device& device, vk::CommandPool& commandPool);
void endSingleSubmitCommand(vk::Device& device, vk::CommandPool& commandPool, vk::CommandBuffer& commandBuffer, vk::Queue& queue);
void transitionImageLayout(VulkanEngine& engine, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
void copyBufferToImage(VulkanEngine& engine, vk::Image image, unsigned char* srcData, vk::Extent3D& extent);

void drawFrame(VulkanEngine& engine);

void destroyEngine(VulkanEngine& engine);

void logVkResult(vk::Result result);

int main() {
  Display display = initDisplay();
  VulkanEngine engine = initEngine(display);
  engine.deleteQueue.push_front([&](){ destroyDisplay(display); });

  vk::Device device = engine.device.device;
  vk::SwapchainKHR swapchain = engine.swapchain.swapchain;

  bool isRunning = true;
  SDL_Event event{};
  while (isRunning) {
    while(SDL_PollEvent(&event)) {
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        isRunning = false;
        break;
      }
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
        isRunning = false;
        break;
      }
    }
    
    drawFrame(engine);
  }
  
  destroyEngine(engine);
}

Display initDisplay() {
  Display display{};

  if (SDL_Init(SDL_INIT_EVERYTHING)) {
    throw std::runtime_error{std::string{"Failed to init SDL: "} + SDL_GetError()};
  }

  if (SDL_Vulkan_LoadLibrary(nullptr)) {
    throw std::runtime_error{std::string{"Failed to init Vulkan for SDL: "} + SDL_GetError()};
  }
  
  SDL_DisplayMode displayMode{};
  if (SDL_GetDisplayMode(0, 0, &displayMode)) {
    throw std::runtime_error{std::string{"Failed to get display mode: "} + SDL_GetError()};
  }
  display.displayMode = displayMode;

  display.window = SDL_CreateWindow("Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, displayMode.w, displayMode.h, SDL_WINDOW_VULKAN);
  if (!display.window) {
    throw std::runtime_error{std::string{"Failed to create SDL window: "} + SDL_GetError()};
  }

  uint32_t extensionCount{};
  if (SDL_Vulkan_GetInstanceExtensions(display.window, &extensionCount, nullptr) == SDL_FALSE) {
    throw std::runtime_error{std::string{"Failed to load SDL Vulkan extensions: "} + SDL_GetError()};
  }

  display.vulkanExtensions.resize(extensionCount);
  if (SDL_Vulkan_GetInstanceExtensions(display.window, &extensionCount, display.vulkanExtensions.data()) == SDL_FALSE) {
    throw std::runtime_error{std::string{"Failed to load SDL Vulkan extensions: "} + SDL_GetError()};
  }

  return display;
}

void destroyDisplay(const Display& display) {
  SDL_DestroyWindow(display.window);
  SDL_Vulkan_UnloadLibrary();
  SDL_Quit();
}

VkSurfaceKHR createVulkanSurface(const Display& display, const VkInstance& instance) {
  VkSurfaceKHR surface{};
  if (!SDL_Vulkan_CreateSurface(display.window, instance, &surface)) {
    throw std::runtime_error{std::string{"Failed to create Vulkan/SDL surface: "} + SDL_GetError()};
  }
  return surface;
};

VulkanEngine initEngine(const Display& display) {
  VulkanEngine engine = {.display=display,.MAX_CONCURRENT_FRAMES=2,.frame=0};

  engine.clearValue = vk::ClearValue{}
      .setColor(vk::ClearColorValue{}.setUint32({0xFF, 0XFF, 0xFF, 0xFF}));

  glm::mat4 world{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};

  glm::vec3 up{1.0f, 1.0f, 1.0f};
  glm::vec3 pos{0.0, 0.0f, 0.0f};
  glm::vec3 direction{0.0, 0.0f, 1.0f};
  
  //world = glm::rotate(world, glm::radians(90.0f), glm::vec3{0.0f, 0.0f, 1.0f});
  //view = glm::lookAt(pos, direction, up);
  //proj = glm::perspective(glm::radians(90.0f), engine.display.displayMode.w / (float) display.displayMode.h, 0.1f, 10.0f);
  //proj[1][1] *= -1;

  Projection projection{world, view, proj};

  createInstance(engine);
  pickDevices(engine);
  createAllocator(engine);
  createSwapchain(engine);
  createSyncPrimitives(engine);
  createQueues(engine);
  createImages(engine);
  createViewportAndScissors(engine);
  createVertexAndIndexBuffers(engine);
  createShaderModules(engine);
  createCommandBuffers(engine);
  createTexture("./textures/brick.jpg", engine);
  createUniformBuffer(engine, projection);
  createDescriptors(engine);
  createGraphicsPipeline(engine);

  return engine;
}

std::vector<char> readFile(const char* path) {
  std::ifstream file{path, std::ios::in | std::ios::binary | std::ios::ate};
  std::ifstream::pos_type size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> bytes(size);
  file.read(bytes.data(), size);

  return bytes;
};


void createInstance(VulkanEngine& engine) {
  vkb::Result<vkb::Instance> instanceResult = vkb::InstanceBuilder{}
    .set_app_name("VkRenderer")
    .require_api_version(1, 3)
    .enable_extensions(engine.display.vulkanExtensions)
    .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
    .enable_validation_layers(true)
    .use_default_debug_messenger()
    .build();
  
  if (!instanceResult) {
    throw std::runtime_error{"Failed to create instance: " + instanceResult.error().message()};
  }

  engine.instance = instanceResult.value();
  engine.deleteQueue.push_back([&]() { vkb::destroy_instance(engine.instance); });
};

void pickDevices(VulkanEngine& engine) {
  engine.surface = createVulkanSurface(engine.display, engine.instance.instance);
  engine.deleteQueue.push_back([&]() { vkb::destroy_surface(engine.instance, engine.surface); });

  vkb::Result<vkb::PhysicalDevice> physicalDeviceResult = vkb::PhysicalDeviceSelector{engine.instance}
    .set_surface(engine.surface)
    .set_minimum_version(1, 3)
    .require_present(true)
    .add_required_extension_features(vk::PhysicalDeviceDynamicRenderingFeatures{}.setDynamicRendering(1))
    .add_required_extension_features(vk::PhysicalDeviceSynchronization2Features{}.setSynchronization2(1))
    .select();

  if (!physicalDeviceResult) {
    throw std::runtime_error{"Failed to select physical device: " + physicalDeviceResult.error().message()};
  }

  engine.physicalDevice = physicalDeviceResult.value();

  vkb::Result<vkb::Device> deviceResult = vkb::DeviceBuilder{engine.physicalDevice}
    .build();

  if (!deviceResult) {
    throw std::runtime_error{"Failed to create a device: " + deviceResult.error().message()};
  }

  engine.device = deviceResult.value();
  engine.deleteQueue.push_back([&]() { vkb::destroy_device(engine.device); });
};

void createAllocator(VulkanEngine& engine) {
  VmaVulkanFunctions vulkanFunctions{};
  vulkanFunctions.vkGetDeviceProcAddr = engine.instance.fp_vkGetDeviceProcAddr;
  vulkanFunctions.vkGetInstanceProcAddr = engine.instance.fp_vkGetInstanceProcAddr;

  VmaAllocatorCreateInfo createInfo{};
  createInfo.pVulkanFunctions = &vulkanFunctions;
  createInfo.instance = engine.instance.instance;
  createInfo.physicalDevice = engine.physicalDevice.physical_device;
  createInfo.device = engine.device.device;
 
  if (vmaCreateAllocator(&createInfo, &engine.allocator) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create a VmaAllocator"};
  };

  engine.deleteQueue.push_back([&]() { vmaDestroyAllocator(engine.allocator); });
};

void createSwapchain(VulkanEngine& engine) {
  vkb::Result<vkb::Swapchain> swapchainResult = vkb::SwapchainBuilder{engine.device}
    .set_desired_extent(engine.display.displayMode.w, engine.display.displayMode.h)
    .set_required_min_image_count(engine.MAX_CONCURRENT_FRAMES)
    .set_desired_present_mode(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
    .build();
  
  if (!swapchainResult) {
    throw std::runtime_error{"Failed to create a swapchain: " + swapchainResult.error().message()};
  }

  engine.swapchain = swapchainResult.value(); 

  engine.deleteQueue.push_back([&]() { 
    vk::Device device = engine.device.device;
    device.destroySwapchainKHR(engine.swapchain.swapchain);
  });
}

void createSyncPrimitives(VulkanEngine& engine) {
  engine.fences.resize(engine.MAX_CONCURRENT_FRAMES);
  engine.renderCompleteSemaphores.resize(engine.MAX_CONCURRENT_FRAMES);
  engine.presentCompleteSemaphores.resize(engine.MAX_CONCURRENT_FRAMES);

  vk::Device device = engine.device.device;

  for (size_t i = 0; i < engine.MAX_CONCURRENT_FRAMES; i++) {
    engine.fences[i] = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
    engine.renderCompleteSemaphores[i] = device.createSemaphore(vk::SemaphoreCreateInfo{});
    engine.presentCompleteSemaphores[i] = device.createSemaphore(vk::SemaphoreCreateInfo{});
  }

  engine.deleteQueue.push_back([&](){
    vk::Device device = engine.device.device;
    for (size_t i = 0; i < engine.MAX_CONCURRENT_FRAMES; i++) {
      device.destroyFence(engine.fences[i]);
      device.destroySemaphore(engine.renderCompleteSemaphores[i]);
      device.destroySemaphore(engine.presentCompleteSemaphores[i]);
    }
  });
}

void createQueues(VulkanEngine& engine) {
  vkb::Result<uint32_t> queueIndexResult = engine.device.get_queue_index(vkb::QueueType::graphics);

  if (!queueIndexResult) {
    throw std::runtime_error{"Failed to get a queue index"  + queueIndexResult.error().message()};
  }

  vkb::Result<VkQueue> queueResult = engine.device.get_queue(vkb::QueueType::graphics);

  if (!queueResult) {
    throw std::runtime_error{"Failed to get a queue"  + queueResult.error().message()};
  }

  engine.queue = queueResult.value();
  engine.queueIndex = queueIndexResult.value();
}

void createImages(VulkanEngine& engine) {
  vkb::Result<std::vector<VkImageView>> imageViewsResult = engine.swapchain.get_image_views();
  vkb::Result<std::vector<VkImage>> imagesResult = engine.swapchain.get_images();

  if (!imageViewsResult) {
    throw std::runtime_error{"Failed to get image views from swapchain"};
  }

  if (!imagesResult) {
    throw std::runtime_error{"Failed to get imags from swapchain"};
  }


  engine.imageViews = imageViewsResult.value();
  engine.images = imagesResult.value();

  engine.deleteQueue.push_back([&](){
    vk::Device device = engine.device.device;
    for (size_t i = 0; i < engine.MAX_CONCURRENT_FRAMES; i++) {
      device.destroyImageView(engine.imageViews[i]);
    }
  });
}

void createViewportAndScissors(VulkanEngine& engine) {
  engine.viewport = vk::Viewport{}
    .setX(0)
    .setY(0)
    .setWidth(engine.swapchain.extent.width)
    .setHeight(engine.swapchain.extent.height)
    .setMaxDepth(1.0f)
    .setMinDepth(0.0f);

  engine.scissors = vk::Rect2D{}
    .setExtent(engine.swapchain.extent)
    .setOffset(0);
};

void createVertexAndIndexBuffers(VulkanEngine& engine) {
  VkBufferCreateInfo vertexBufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(sizeof(Vertex) * vertices.size())
    .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
    .setSharingMode(vk::SharingMode::eExclusive);

  VmaAllocationCreateInfo vertexAllocationCreateInfo{};
  vertexAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  vertexAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer vertexBuffer{};
  VmaAllocation vertexBufferAllocation{};

  vmaCreateBuffer(engine.allocator, &vertexBufferCreateInfo, &vertexAllocationCreateInfo, &vertexBuffer, &vertexBufferAllocation, nullptr);
  vmaCopyMemoryToAllocation(engine.allocator, vertices.data(), vertexBufferAllocation, 0, sizeof(Vertex) * vertices.size());
  
  engine.vertexBuffer.buffer = vertexBuffer;
  engine.vertexBuffer.allocation = vertexBufferAllocation;

  engine.deleteQueue.push_back([&]() { vmaDestroyBuffer(engine.allocator, engine.vertexBuffer.buffer, engine.vertexBuffer.allocation); });

  VkBufferCreateInfo indexBufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(sizeof(uint16_t) * indices.size())
    .setUsage(vk::BufferUsageFlagBits::eIndexBuffer)
    .setSharingMode(vk::SharingMode::eExclusive);

  VmaAllocationCreateInfo indexAllocationCreateInfo{};
  indexAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  indexAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer indexBuffer{};
  VmaAllocation indexBufferAllocation{};

  vmaCreateBuffer(engine.allocator, &indexBufferCreateInfo, &indexAllocationCreateInfo, &indexBuffer, &indexBufferAllocation, nullptr);
  vmaCopyMemoryToAllocation(engine.allocator, indices.data(), indexBufferAllocation, 0, sizeof(uint16_t) * indices.size());

  engine.indexBuffer.buffer = indexBuffer;
  engine.indexBuffer.allocation = indexBufferAllocation;

  engine.deleteQueue.push_back([&]() { vmaDestroyBuffer(engine.allocator, engine.indexBuffer.buffer, engine.indexBuffer.allocation); });
};

void createShaderModules(VulkanEngine& engine) {
  std::vector<char> vertexShaderFile = readFile("./shaders/vert.spv");
  std::vector<char> fragmentShaderFile = readFile("./shaders/frag.spv");

  vk::ShaderModuleCreateInfo vertexShaderCreateInfo = vk::ShaderModuleCreateInfo{}
    .setPCode(reinterpret_cast<uint32_t*>(vertexShaderFile.data()))
    .setCodeSize(vertexShaderFile.size());

  vk::ShaderModuleCreateInfo fragmentShaderCreateInfo = vk::ShaderModuleCreateInfo{}
    .setPCode(reinterpret_cast<uint32_t*>(fragmentShaderFile.data()))
    .setCodeSize(fragmentShaderFile.size());

  vk::Device device = engine.device.device;
  
  engine.vertexShader = device.createShaderModule(vertexShaderCreateInfo, VK_NULL_HANDLE);
  engine.deleteQueue.push_back([&]() { vk::Device{engine.device.device}.destroyShaderModule(engine.vertexShader); });

  engine.fragmentShader = device.createShaderModule(fragmentShaderCreateInfo, VK_NULL_HANDLE);
  engine.deleteQueue.push_back([&]() { vk::Device{engine.device.device}.destroyShaderModule(engine.fragmentShader); });
};

void createCommandBuffers(VulkanEngine& engine) {
  vk::CommandPoolCreateInfo commandPoolCreateInfo = vk::CommandPoolCreateInfo{}
    .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
    .setQueueFamilyIndex(engine.queueIndex);

  vk::Device device = engine.device.device;

  engine.commandPool = device.createCommandPool(commandPoolCreateInfo, nullptr);
  engine.deleteQueue.push_back([&]() { vk::Device{engine.device.device}.destroyCommandPool(engine.commandPool); });

  engine.commadBuffers.resize(engine.MAX_CONCURRENT_FRAMES);  

  vk::CommandBufferAllocateInfo commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{}
    .setCommandPool(engine.commandPool)
    .setCommandBufferCount(engine.MAX_CONCURRENT_FRAMES)
    .setLevel(vk::CommandBufferLevel::ePrimary);
  
  if (device.allocateCommandBuffers(&commandBufferAllocateInfo, engine.commadBuffers.data()) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to allocate a command buffer"};
  };
};

void createDescriptors(VulkanEngine& engine) {
  vk::Device device = engine.device.device;

  vk::DescriptorPoolSize samplerPool = vk::DescriptorPoolSize{}
    .setDescriptorCount(engine.MAX_CONCURRENT_FRAMES)
    .setType(vk::DescriptorType::eCombinedImageSampler);

  vk::DescriptorPoolSize uniformPool = vk::DescriptorPoolSize{}
    .setDescriptorCount(engine.MAX_CONCURRENT_FRAMES)
    .setType(vk::DescriptorType::eUniformBuffer);

  std::vector<vk::DescriptorPoolSize> poolSizes{
    samplerPool,
    uniformPool
  };

  vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{}
    .setMaxSets(engine.MAX_CONCURRENT_FRAMES)
    .setPoolSizes(poolSizes)
    .setPoolSizeCount(poolSizes.size());

  engine.descriptorPool = device.createDescriptorPool(descriptorPoolCreateInfo, nullptr);

  vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo{}
    .setMagFilter(vk::Filter::eLinear)
    .setMinFilter(vk::Filter::eLinear)
    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
    .setAnisotropyEnable(0)
    .setCompareEnable(0);

  engine.sampler = device.createSampler(samplerCreateInfo);

  vk::DescriptorSetLayoutBinding samplerBinding = vk::DescriptorSetLayoutBinding{}
    .setBinding(0)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eFragment)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
    .setImmutableSamplers(engine.sampler);

  vk::DescriptorSetLayoutBinding uniformBinding = vk::DescriptorSetLayoutBinding{}
    .setBinding(1)
    .setDescriptorCount(1)
    .setStageFlags(vk::ShaderStageFlagBits::eVertex)
    .setDescriptorType(vk::DescriptorType::eUniformBuffer);

  std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{
    samplerBinding,
    uniformBinding
  };

  vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{}
    .setBindings(setLayoutBindings)
    .setBindingCount(setLayoutBindings.size());

  engine.descriptorSetLayout = device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo, nullptr);

  std::vector<vk::DescriptorSetLayout> layouts(engine.MAX_CONCURRENT_FRAMES, engine.descriptorSetLayout); 

  vk::DescriptorSetAllocateInfo allocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorPool(engine.descriptorPool)
    .setDescriptorSetCount(engine.MAX_CONCURRENT_FRAMES)
    .setSetLayouts(layouts);

  engine.descriptorSets = device.allocateDescriptorSets(allocateInfo);

  for (size_t i = 0; i < engine.MAX_CONCURRENT_FRAMES; i++) {
    vk::DescriptorImageInfo imageInfo = vk::DescriptorImageInfo{}
      .setSampler(engine.sampler) 
      .setImageView(engine.texture.imageView)
      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::DescriptorBufferInfo bufferInfo = vk::DescriptorBufferInfo{}
      .setBuffer(engine.uniformBuffer.buffer)
      .setRange(sizeof(Projection))
      .setOffset(0);

    vk::WriteDescriptorSet samplerWrite = vk::WriteDescriptorSet{}
      .setDstSet(engine.descriptorSets[i])
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setImageInfo(imageInfo);
    
    vk::WriteDescriptorSet bufferWrite = vk::WriteDescriptorSet{}
      .setDstSet(engine.descriptorSets[i])
      .setDstBinding(1)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setBufferInfo(bufferInfo);

    std::vector<vk::WriteDescriptorSet> writes{
      samplerWrite,
      bufferWrite,
    };

    device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
  }

  engine.deleteQueue.push_back([&]() { 
    vk::Device d = engine.device.device;
    d.destroyDescriptorSetLayout(engine.descriptorSetLayout); 
    d.destroySampler(engine.sampler);
    d.destroyDescriptorPool(engine.descriptorPool); 
  });
}

void createGraphicsPipeline(VulkanEngine& engine) {
  vk::Device device = engine.device.device;

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

  vk::PipelineShaderStageCreateInfo vertexShader = vk::PipelineShaderStageCreateInfo{}
    .setStage(vk::ShaderStageFlagBits::eVertex)
    .setModule(engine.vertexShader)
    .setPName("main")
    .setPSpecializationInfo(nullptr);

  vk::PipelineShaderStageCreateInfo fragmentShader = vk::PipelineShaderStageCreateInfo{}
    .setStage(vk::ShaderStageFlagBits::eFragment)
    .setModule(engine.fragmentShader)
    .setPName("main")
    .setPSpecializationInfo(nullptr);
  
  vk::Format colorAttachmentFormat = vk::Format::eB8G8R8A8Srgb;

  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{}
    .setColorAttachmentCount(1)
    .setColorAttachmentFormats(colorAttachmentFormat);
  
  std::vector<vk::PipelineShaderStageCreateInfo> stages{
    vertexShader,
    fragmentShader,
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
    .setScissors(engine.scissors)
    .setViewports(engine.viewport)
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
    .setDepthTestEnable(0)
    .setDepthWriteEnable(0)
    .setStencilTestEnable(0)
    .setDepthBoundsTestEnable(0)
    .setDepthCompareOp(vk::CompareOp::eNever);

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
      .setSetLayouts(engine.descriptorSetLayout)
      .setSetLayoutCount(1);

  engine.pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo, nullptr);
  engine.deleteQueue.push_back([&]() { vk::Device{engine.device.device}.destroyPipelineLayout(engine.pipelineLayout); });
    
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
    .setLayout(engine.pipelineLayout);

  vk::ResultValue<vk::Pipeline> pipelineResult = device.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);

  if (pipelineResult.result != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to create a pipeline"};
  }

  engine.graphicsPipeline = pipelineResult.value;
  engine.deleteQueue.push_back([&]() { vk::Device{engine.device.device}.destroyPipeline(engine.graphicsPipeline); });
};

void drawFrame(VulkanEngine& engine) {
    vk::Device device = engine.device.device;
    vk::SwapchainKHR swapchain = engine.swapchain.swapchain;

    uint16_t& frame = engine.frame;

    if (device.waitForFences(1, &engine.fences[frame], 1, UINT64_MAX) != vk::Result::eSuccess) {
      throw std::runtime_error{"Failed to wait for fence"};
    };

    if (device.resetFences(1, &engine.fences[frame]) != vk::Result::eSuccess) {
      throw std::runtime_error{"Failed to reset fence"};
    };

    vk::CommandBuffer commandBuffer = engine.commadBuffers[frame];
    vk::DescriptorSet descriptorSet = engine.descriptorSets[frame];

    commandBuffer.reset();

    vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo{}
      .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);
   
    uint32_t imageIndex;
    vk::Result acquireResult = device.acquireNextImageKHR(swapchain, UINT64_MAX, engine.presentCompleteSemaphores[frame], nullptr, &imageIndex);
    
    switch (acquireResult) {
      case vk::Result::eSuccess:
        break;
      case vk::Result::eSuboptimalKHR:
        break;
      case vk::Result::eErrorOutOfDateKHR:
        throw std::runtime_error{"Out of date swapchain"};
        break;
      case vk::Result::eNotReady:
      default:
        throw std::runtime_error{"Failed to acquire next image"};
        break;
    }

    vk::RenderingAttachmentInfo attachment = vk::RenderingAttachmentInfo{}
      .setImageView(engine.imageViews[frame])
      .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setClearValue(engine.clearValue);

    vk::RenderingInfo renderingInfo = vk::RenderingInfo{}
      .setRenderArea(engine.scissors)
      .setLayerCount(1)
      .setViewMask(0)
      .setColorAttachmentCount(1)
      .setColorAttachments(attachment);

    vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange{}
      .setLayerCount(1)
      .setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setBaseMipLevel(0)
      .setLevelCount(1)
      .setBaseArrayLayer(0);

    vk::ImageMemoryBarrier2 imageMemoryBarrier = vk::ImageMemoryBarrier2{}
      .setImage(engine.images[frame])
      .setOldLayout(vk::ImageLayout::eUndefined)
      .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setSrcAccessMask(vk::AccessFlagBits2::eNone)
      .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
      .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
      .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
      .setSubresourceRange(subresourceRange);

    vk::ImageMemoryBarrier2 presentImageMemoryBarrier = vk::ImageMemoryBarrier2{}
      .setImage(engine.images[frame])
      .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
      .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
      .setDstAccessMask(vk::AccessFlagBits2::eNone)
      .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
      .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
      .setSubresourceRange(subresourceRange);

    vk::ImageMemoryBarrier2 imageMemoryBarriers[2] = {imageMemoryBarrier, presentImageMemoryBarrier};

    vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}
      .setImageMemoryBarriers(imageMemoryBarriers)
      .setImageMemoryBarrierCount(2);
    
    commandBuffer.pipelineBarrier2(dependencyInfo);

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.setViewport(0, 1, &engine.viewport);
    commandBuffer.setScissor(0, 1, &engine.scissors);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, engine.graphicsPipeline);
    
    vk::Buffer buffers[1] = {engine.vertexBuffer.buffer};
    vk::DeviceSize offsets[1] = {0};
    
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, engine.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    commandBuffer.bindVertexBuffers(0, 1, buffers, offsets);
    commandBuffer.bindIndexBuffer(engine.indexBuffer.buffer, 0, vk::IndexType::eUint16);

    commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 1);

    commandBuffer.endRendering();

    commandBuffer.end();

    vk::Flags<vk::PipelineStageFlagBits> waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    
    vk::SubmitInfo submitInfo = vk::SubmitInfo{}
      .setWaitSemaphoreCount(1)
      .setWaitSemaphores(engine.presentCompleteSemaphores[frame])
      .setCommandBuffers(commandBuffer)
      .setCommandBufferCount(1)
      .setSignalSemaphores(engine.renderCompleteSemaphores[frame])
      .setSignalSemaphoreCount(1)
      .setWaitDstStageMask(waitStage);

    if (engine.queue.submit(1, &submitInfo, engine.fences[frame]) != vk::Result::eSuccess) {
      throw std::runtime_error{"Failed to submit to queue"};
    };

    uint32_t imageIndices = {imageIndex};

    vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR{}
      .setWaitSemaphoreCount(1)
      .setWaitSemaphores(engine.renderCompleteSemaphores[frame])
      .setSwapchainCount(1)
      .setSwapchains(swapchain)
      .setImageIndices(imageIndices);

    vk::Result presentResult = engine.queue.presentKHR(&presentInfo);

    switch (presentResult) {
      case vk::Result::eSuccess:
        break;
      case vk::Result::eSuboptimalKHR:
        break;
      case vk::Result::eErrorOutOfDateKHR:
        throw std::runtime_error{"Out of date swapchain"};
        break;
      case vk::Result::eNotReady:
        throw std::runtime_error{"Not ready swapchain"};
        break;
      default:
        throw std::runtime_error{"Failed to presentKHR"};
        break;
    }

    frame = (frame + 1) % engine.MAX_CONCURRENT_FRAMES;
}

void destroyEngine(VulkanEngine& engine) {
  vk::Device device = engine.device.device;

  device.waitIdle();
    
  while (!engine.deleteQueue.empty()) {
    engine.deleteQueue.back()();
    engine.deleteQueue.pop_back();
  }
}

void createTexture(const char* path, VulkanEngine& engine) {
  vk::Device device = engine.device.device;

  unsigned char* data = stbi_load(path, &engine.texture.w, &engine.texture.h, nullptr, STBI_rgb_alpha);

  vk::Extent3D extent = vk::Extent3D{}
    .setWidth(engine.texture.w)
    .setHeight(engine.texture.h)
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

  VkImage image{};
  VmaAllocation imageAllocation{};

  vmaCreateImage(engine.allocator, &imageCreateInfo, &imageAllocationCreateInfo, &image, &imageAllocation, nullptr);

  engine.texture.image = image;
  engine.texture.allocation = imageAllocation;

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
  
  engine.texture.imageView = device.createImageView(imageViewCreateInfo, nullptr);

  transitionImageLayout(engine, engine.texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  copyBufferToImage(engine, engine.texture.image, data, extent);
  transitionImageLayout(engine, engine.texture.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

  stbi_image_free(data);

  engine.deleteQueue.push_back([&]() { 
    vk::Device{engine.device.device}.destroyImageView(engine.texture.imageView);
    vmaDestroyImage(engine.allocator, engine.texture.image, engine.texture.allocation);
  });

}

void createUniformBuffer(VulkanEngine& engine, Projection& projection) {
  VkBufferCreateInfo uniformBufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(sizeof(Projection))
    .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
    .setSharingMode(vk::SharingMode::eExclusive);

  VmaAllocationCreateInfo uniformAllocationCreateInfo{};
  uniformAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  uniformAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer uniformBuffer{};
  VmaAllocation uniformBufferAllocation{};

  vmaCreateBuffer(engine.allocator, &uniformBufferCreateInfo, &uniformAllocationCreateInfo, &uniformBuffer, &uniformBufferAllocation, nullptr);
  vmaCopyMemoryToAllocation(engine.allocator, &projection, uniformBufferAllocation, 0, sizeof(Projection));

  engine.uniformBuffer.buffer = uniformBuffer;
  engine.uniformBuffer.allocation = uniformBufferAllocation;

  engine.deleteQueue.push_back([&]() { vmaDestroyBuffer(engine.allocator, engine.uniformBuffer.buffer, engine.uniformBuffer.allocation); });
};

vk::CommandBuffer beginSingleSubmitCommand(vk::Device& device, vk::CommandPool& commandPool) {
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
};

void endSingleSubmitCommand(vk::Device& device, vk::CommandPool& commandPool, vk::CommandBuffer& commandBuffer, vk::Queue& queue) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo = vk::SubmitInfo{}
    .setCommandBuffers(commandBuffer)
    .setCommandBufferCount(1);

  if (queue.submit(1, &submitInfo, nullptr) != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to submit to queue"};
  };

  queue.waitIdle();
  device.freeCommandBuffers(commandPool, 1, &commandBuffer);
};

void transitionImageLayout(VulkanEngine& engine, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
  vk::Device device = engine.device.device;

  vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange{}
    .setLayerCount(1)
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseMipLevel(0)
    .setLevelCount(1)
    .setBaseArrayLayer(0);

  vk::ImageMemoryBarrier2 imageMemoryBarrier = vk::ImageMemoryBarrier2{}
    .setImage(engine.texture.image)
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

  vk::CommandBuffer commandBuffer = beginSingleSubmitCommand(device, engine.commandPool);
  
  commandBuffer.pipelineBarrier2(dependencyInfo);

  endSingleSubmitCommand(device, engine.commandPool, commandBuffer, engine.queue);
};

void copyBufferToImage(VulkanEngine& engine, vk::Image image, unsigned char* srcData, vk::Extent3D& extent) {
  vk::Device device = engine.device.device;
  vk::DeviceSize size = extent.width * extent.height * sizeof(unsigned char) * STBI_rgb_alpha;
  
  VmaAllocationCreateInfo stagingBufferAllocationCreateInfo{};
  stagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; 
  stagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBufferCreateInfo stagingBufferCreateInfo = vk::BufferCreateInfo{}
    .setSize(size)
    .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
    .setSharingMode(vk::SharingMode::eExclusive);

  VkBuffer stagingBuffer{};
  VmaAllocation stagingBufferAllocation{}; 

  vmaCreateBuffer(engine.allocator, &stagingBufferCreateInfo, &stagingBufferAllocationCreateInfo, &stagingBuffer, &stagingBufferAllocation, nullptr);
  vmaCopyMemoryToAllocation(engine.allocator, srcData, stagingBufferAllocation, 0, size);

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
    .setSrcBuffer(stagingBuffer)
    .setDstImage(image)
    .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
    .setRegionCount(1)
    .setRegions(copyRegion);

  vk::CommandBuffer commandBuffer = beginSingleSubmitCommand(device, engine.commandPool);

  commandBuffer.copyBufferToImage2(copyInfo);

  endSingleSubmitCommand(device, engine.commandPool, commandBuffer, engine.queue);

  vmaDestroyBuffer(engine.allocator, stagingBuffer, stagingBufferAllocation);
};
