#pragma once

#include "vk-pipeline.hpp"
#include "buffer.hpp"
#include "mesh.hpp"
#include "scene.hpp"
#include <glm/ext/vector_float3.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

Pipeline::Pipeline(
  const VmaAllocator& allocator,
  const Shader& vert, 
  const Shader& frag, 
  const vk::Device& device, 
  const vk::Viewport& v, 
  const vk::Rect2D& s, 
  const uint32_t swapImgCount,
  const vk::DescriptorPool& descriporPool,
  const vk::DescriptorSetLayout& descSetLayout,
  const vk::DescriptorSetLayout& texSetLayout
): vertexShader{vert}, fragmentShader{frag}, descriptorSetLayout{descSetLayout}, textureSetLayout{texSetLayout}, swapchainImageCount{swapImgCount}, viewport{v}, scissors{s} {
  createVertexInputState();
  createDescriptors(descriporPool, descriptorSetLayout, allocator, device);
  createPipeline(device);
};

void Pipeline::createVertexInputState() {
  inputBindings.push_back(
    vk::VertexInputBindingDescription{}
      .setStride(sizeof(Vertex))
      .setInputRate(vk::VertexInputRate::eVertex)
      .setBinding(0)
  );

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

  vk::VertexInputAttributeDescription vertexNormalsAttributeDescription = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setLocation(3)
    .setOffset(offsetof(Vertex, normals))
    .setFormat(vk::Format::eR32G32B32Sfloat);

  inputAttributes = {
    vertexPositionAttributeDescription, 
    vertexColorAttributeDescription, 
    vertexTextureCoordAttributeDescription,
    vertexNormalsAttributeDescription,
  };
}

void Pipeline::createDescriptors(const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const VmaAllocator& allocator, const vk::Device& device) {
  std::vector<vk::DescriptorSetLayout> layouts(swapchainImageCount, descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocateInfo = vk::DescriptorSetAllocateInfo{}
    .setDescriptorPool(descriptorPool)
    .setDescriptorSetCount(swapchainImageCount)
    .setSetLayouts(descriptorSetLayout);

  descriptorSets = device.allocateDescriptorSets(allocateInfo);

  binding0 = Buffer{allocator, sizeof(Projection), vk::BufferUsageFlagBits::eUniformBuffer};
  binding1 = Buffer{allocator, sizeof(glm::vec3), vk::BufferUsageFlagBits::eUniformBuffer};

  for (size_t i = 0; i < descriptorSets.size(); i++) {
    vk::DescriptorBufferInfo projInfo = vk::DescriptorBufferInfo{}
      .setBuffer(binding0.buffer)
      .setRange(sizeof(Projection))
      .setOffset(0);

    vk::DescriptorBufferInfo posInfo = vk::DescriptorBufferInfo{}
      .setBuffer(binding1.buffer)
      .setRange(sizeof(glm::vec3))
      .setOffset(0);
    
    vk::WriteDescriptorSet projWrite = vk::WriteDescriptorSet{}
      .setDstSet(descriptorSets[i])
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setBufferInfo(projInfo);

    vk::WriteDescriptorSet posWrite = vk::WriteDescriptorSet{}
      .setDstSet(descriptorSets[i])
      .setDstBinding(1)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setBufferInfo(posInfo);

    std::vector<vk::WriteDescriptorSet> writes{
      projWrite,
      posWrite
    };

    device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
  }
}

void Pipeline::createPipeline(const vk::Device& device) {
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
  
  vk::PipelineVertexInputStateCreateInfo vertexInputState = vk::PipelineVertexInputStateCreateInfo{}
    .setVertexBindingDescriptions(inputBindings)
    .setVertexBindingDescriptionCount(inputBindings.size())
    .setVertexAttributeDescriptionCount(inputAttributes.size())
    .setVertexAttributeDescriptions(inputAttributes);

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

  std::vector<vk::DescriptorSetLayout> setLayouts = {textureSetLayout, descriptorSetLayout};

  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo{}
      .setSetLayouts(setLayouts)
      .setSetLayoutCount(setLayouts.size());

  pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo, nullptr);
    
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

  vk::ResultValue<vk::Pipeline> pipelineResult = device.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);

  if (pipelineResult.result != vk::Result::eSuccess) {
    throw std::runtime_error{"Failed to create a pipeline"};
  }

  graphicsPipeline = pipelineResult.value;
}
