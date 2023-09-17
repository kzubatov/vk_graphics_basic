#include "deferred_render.hpp"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>

// #ifndef STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// #endif

/// RESOURCE ALLOCATION

void DeferredRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
  m_cam.nearPlane = loadedCam.nearPlane;
}

void DeferredRender::LoadTextures() {
  // std::string_view name;
  // vk::Format format = vk::Format::eR8G8B8A8Srgb;
  // vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
  // VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
  // vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
  // std::size_t layers = 1;
  // std::size_t mipLevels = 1;
  // vk::Filter filter = vk::Filter::eNearest;
  // vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eClampToEdge;
  // float minLod = 0.0f;
  etna::TextureLoader::TextureInfo commonInfo = {
    .name = {},
    .format = {},
    .imageUsage = vk::ImageUsageFlagBits::eSampled,
    .mipLevels = SIZE_MAX,
    .filter = vk::Filter::eLinear,
    .addressMode = vk::SamplerAddressMode::eRepeat,
  };

  commonInfo.name = "bunnyAlbedo";
  commonInfo.format = vk::Format::eR8G8B8A8Srgb;
  bunnyAlbedoTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/plastic/alb6.png");
  
  commonInfo.name = "bunnyNormal";
  commonInfo.format = vk::Format::eR8G8B8A8Unorm;
  bunnyNormalTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/plastic/normal.png");
    
  commonInfo.name = "bunnyRoughness";
  commonInfo.format = vk::Format::eR8Unorm;
  bunnyRoughnessTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/plastic/roughness.png");
  
  commonInfo.name = "teapotAlbedo";
  commonInfo.format = vk::Format::eR8G8B8A8Srgb;
  teapotAlbedoTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rusted-steel/albedo.png");
  
  commonInfo.name = "teapotNormal";
  commonInfo.format = vk::Format::eR8G8B8A8Unorm;
  teapotNormalTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rusted-steel/normal.png");
  
  commonInfo.name = "teapotMetalness";
  commonInfo.format = vk::Format::eR8Unorm;
  teapotMetalnessTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rusted-steel/metalness.png");
  
  commonInfo.name = "teapotRoughness";
  teapotRoughnessTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rusted-steel/roughness.png");
  
  commonInfo.name = "flatAlbedo";
  commonInfo.format = vk::Format::eR8G8B8A8Srgb;
  flatAlbedoTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rock/albedo.png");
  
  commonInfo.name = "flatNormal";
  commonInfo.format = vk::Format::eR8G8B8A8Unorm;
  flatNormalTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rock/normal.png");
  
  commonInfo.name = "flatRoughness";
  commonInfo.format = vk::Format::eR8Unorm;
  flatRoughnessTexture = m_pTextureLoader->load(commonInfo, VK_GRAPHICS_BASIC_ROOT"/resources/textures/rock/roughness.png");
}

// Texture::Texture(const char *a_name, int a_channelCount, vk::Format a_format) : name(a_name), 
//   channelCount(a_channelCount), format(a_format) {} 

// Texture::~Texture() {
//   isLoaded = false;
//   texture.reset();
//   copyBuffer = etna::Buffer();
// }

// void Texture::load(VkCommandBuffer a_cmdBuff, const char *path, etna::GlobalContext *a_context) {
//   if (!isLoaded) {
//     isLoaded = true;

//     int texW, texH, texC;
//     auto pixels = stbi_load(path, &texW, &texH, &texC, channelCount);

//     std::cout << path << std::endl;
//     ETNA_ASSERT(pixels != nullptr);

//     mipLevels = (uint32_t) floor(log2(std::max(texW, texH))) + 1u;

//     copyBuffer = a_context->createBuffer({
//       .size = (uint32_t) texW * texH * channelCount,
//       .bufferUsage = vk::BufferUsageFlagBits::eTransferSrc,
//       .memoryUsage = VMA_MEMORY_USAGE_CPU_COPY,
//       .name = "copyBuffer",
//     });

//     auto copyBufferMap = copyBuffer.map();
//     memcpy(copyBufferMap, pixels, texW * texH * channelCount);
//     copyBuffer.unmap();
//     stbi_image_free(pixels);

//     texture = a_context->createImage({
//       .extent = { 
//         .width = static_cast<uint32_t>(texW),
//         .height = static_cast<uint32_t>(texH),
//         .depth = 1U,
//       },
//       .name = name,
//       .format = format,
//       .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
//       .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
//       .tiling = vk::ImageTiling::eOptimal,
//       .layers = 1,
//       .mipLevels = mipLevels,
//     });

//     VkBufferImageCopy info = {
//       .bufferOffset = 0,
//       .bufferRowLength = 0,
//       .bufferImageHeight = 0,
//       .imageSubresource = {
//         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//         .mipLevel = 0,
//         .baseArrayLayer = 0,
//         .layerCount = 1,
//       },
//       .imageOffset = { .x = 0, .y = 0, .z = 0 },
//       .imageExtent = { 
//         .width = static_cast<uint32_t>(texW),
//         .height = static_cast<uint32_t>(texH),
//         .depth = 1U,
//       },
//     };
  
//     VkImageMemoryBarrier barrier = {
//       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//       .pNext = nullptr,
//       .srcAccessMask = 0,
//       .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
//       .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//       .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//       .srcQueueFamilyIndex = a_context->getQueueFamilyIdx(),
//       .dstQueueFamilyIndex = a_context->getQueueFamilyIdx(),
//       .image = texture.get(),
//       .subresourceRange = {
//         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//         .baseMipLevel = 0,
//         .levelCount = mipLevels,
//         .baseArrayLayer = 0,
//         .layerCount = 1,
//       },
//     };

//     vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
//       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

//     vkCmdCopyBufferToImage(a_cmdBuff, copyBuffer.get(), texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &info);

//     barrier.subresourceRange.levelCount = 1;

//     int32_t mipWidth = texW;
//     int32_t mipHeight = texH;

//     for (uint32_t i = 1; i < mipLevels; i++) {
//       barrier.subresourceRange.baseMipLevel = i - 1;
//       barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//       barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//       barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//       barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

//       vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, 
//         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

//       VkImageBlit blit{};
//       blit.srcOffsets[0] = {0, 0, 0};
//       blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
//       blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//       blit.srcSubresource.mipLevel = i - 1;
//       blit.srcSubresource.baseArrayLayer = 0;
//       blit.srcSubresource.layerCount = 1;
//       blit.dstOffsets[0] = {0, 0, 0};
//       blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth >> 1 : 1, mipHeight > 1 ? mipHeight >> 1 : 1, 1 };
//       blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//       blit.dstSubresource.mipLevel = i;
//       blit.dstSubresource.baseArrayLayer = 0;
//       blit.dstSubresource.layerCount = 1;

//       vkCmdBlitImage(a_cmdBuff, texture.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//         texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

//       barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//       barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//       barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//       barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//       vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, 
//         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

//       if (mipWidth > 1) mipWidth >>= 1;
//       if (mipHeight > 1) mipHeight >>= 1;
//     }

//     barrier.subresourceRange.baseMipLevel = mipLevels - 1;
//     barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//     barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//     vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, 
//       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
//   };
// }

void DeferredRender::PrepareCullingResources() {
  std::vector<std::vector<float4x4>> matrices(m_pScnMgr->MeshesNum());
  std::vector<mInfo> mesh(m_pScnMgr->MeshesNum());
  
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i) {
    matrices[m_pScnMgr->GetInstanceInfo(i).mesh_id].push_back(m_pScnMgr->GetInstanceMatrix(i));
  }

  size_t offset = 0;
  for (uint32_t i = 0; i < m_pScnMgr->MeshesNum(); ++i) {
    size_t increment = sizeof(float4x4) * matrices[i].size();
    memcpy(m_instanceMatricesMap + offset, matrices[i].data(), increment);
    offset += increment;

    float4 a = m_pScnMgr->GetMeshBbox(i).boxMin;
    float4 b = m_pScnMgr->GetMeshBbox(i).boxMax;
    mesh[i].center = (a + b) / 2.0;
    mesh[i].radius = length(a - b) / 2.0;
    mesh[i].count = matrices[i].size();

    if (m_groupCountX < mesh[i].count) {
      m_groupCountX = mesh[i].count;
    }
  }

  memcpy(m_meshInfoMap, mesh.data(), sizeof(mInfo) * mesh.size());
}

void DeferredRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });
  std::cout << "here" << std::endl;

  colorMap = m_context->createImage(etna::Image::CreateInfo {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "colorMap",
    .format = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  normalMap = m_context->createImage(etna::Image::CreateInfo {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "normalMap",
    .format = vk::Format::eR16G16B16A16Snorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

  instanceMatrices = m_context->createBuffer(etna::Buffer::CreateInfo {
    .size = sizeof(LiteMath::float4x4) * m_pScnMgr->InstancesNum(),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_COPY,
    .name = "instanceMatrices",
  });
  m_instanceMatricesMap = instanceMatrices.map();

  meshInfo = m_context->createBuffer(etna::Buffer::CreateInfo {
    .size = sizeof(mInfo) * m_pScnMgr->MeshesNum(),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_COPY,
    .name = "meshInfo",
  });
  m_meshInfoMap = meshInfo.map();
  
  PrepareCullingResources();

  drawIndirectInfo = m_context->createBuffer(etna::Buffer::CreateInfo {
    .size = sizeof(VkDrawIndexedIndirectCommand) * m_pScnMgr->MeshesNum(),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "drawIndirectInfo",
  });

  visibleInstances = m_context->createBuffer(etna::Buffer::CreateInfo {
    .size = sizeof(uint32_t) * m_pScnMgr->InstancesNum(),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "visibleInstances",
  }); 

  // lightBuffer = m_context->createBuffer(etna::Buffer::CreateInfo {
  //   .size = sizeof(lightParams) * lightMax,
  //   .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
  //   .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
  //   .name = "lightBuffer", 
  // });
  // m_lights = (lightParams *) lightBuffer.map();
}

void DeferredRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  colorMap.reset();
  normalMap.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  instanceMatrices = etna::Buffer();
  meshInfo = etna::Buffer();
  drawIndirectInfo = etna::Buffer();
  visibleInstances = etna::Buffer();

  bunnyAlbedoTexture.image.reset();
  bunnyNormalTexture.image.reset();
  bunnyRoughnessTexture.image.reset();

  teapotAlbedoTexture.image.reset();
  teapotNormalTexture.image.reset();
  teapotMetalnessTexture.image.reset();
  teapotRoughnessTexture.image.reset();

  flatAlbedoTexture.image.reset();
  flatNormalTexture.image.reset();
  flatRoughnessTexture.image.reset();

  // lightBuffer.unmap();
  // lightBuffer = etna::Buffer();
}


/// PIPELINES CREATION

void DeferredRender::loadShaders()
{
  etna::create_program("culling", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/culling.comp.spv"});
  etna::create_program("gBuffer", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/gbuffer.frag.spv"});
  etna::create_program("finalPass", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/deferred_shading.frag.spv"});
}

void DeferredRender::SetupPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc {
    .bindings = {etna::VertexShaderInputDescription::Binding {
      .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
    }}
  };

  vk::PipelineColorBlendAttachmentState colorBlendAttachmentState = {
    .blendEnable = false,
    .colorWriteMask = vk::ColorComponentFlagBits::eR
      | vk::ColorComponentFlagBits::eG
      | vk::ColorComponentFlagBits::eB
      | vk::ColorComponentFlagBits::eA
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_gBufferPipeline = pipelineManager.createGraphicsPipeline("gBuffer",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .blendingConfig = 
        {
          .attachments = {2, colorBlendAttachmentState},
          .logicOp = vk::LogicOp::eCopy,
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR8G8B8A8Srgb, vk::Format::eR16G16B16A16Snorm},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });

  m_finalPassPipeline = pipelineManager.createGraphicsPipeline("finalPass", {
    .vertexShaderInput = {},
    .blendingConfig = 
      {
        .attachments = {colorBlendAttachmentState},
        .logicOp = vk::LogicOp::eCopy,
      },
    .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
      },
  });

  m_cullingComputePipeline = pipelineManager.createComputePipeline("culling", {});
}

/// COMMAND BUFFER FILLING

void DeferredRender::CullSceneCmd(VkCommandBuffer a_cmdBuff) {
  auto simpleCullingInfo = etna::get_shader_program("culling");

  VkDeviceSize offset = 0;
  for (uint32_t i = 0; i < m_pScnMgr->MeshesNum(); ++i) {
    auto mesh_info = m_pScnMgr->GetMeshInfo(i);

    vkCmdFillBuffer(a_cmdBuff, drawIndirectInfo.get(), offset + 0,                    sizeof(uint32_t), mesh_info.m_indNum);
    vkCmdFillBuffer(a_cmdBuff, drawIndirectInfo.get(), offset + sizeof(uint32_t),     sizeof(uint32_t), 0);
    vkCmdFillBuffer(a_cmdBuff, drawIndirectInfo.get(), offset + 2 * sizeof(uint32_t), sizeof(uint32_t), mesh_info.m_indexOffset);
    vkCmdFillBuffer(a_cmdBuff, drawIndirectInfo.get(), offset + 3 * sizeof(uint32_t), sizeof(uint32_t), mesh_info.m_vertexOffset);
    vkCmdFillBuffer(a_cmdBuff, drawIndirectInfo.get(), offset + 4 * sizeof(uint32_t), sizeof(uint32_t), 0);

    offset += sizeof(VkDrawIndexedIndirectCommand);
  }

  auto set = etna::create_descriptor_set(simpleCullingInfo.getDescriptorLayoutId(0), a_cmdBuff, 
  {
    etna::Binding {0, instanceMatrices.genBinding()},
    etna::Binding {1, meshInfo.genBinding()},
    etna::Binding {2, visibleInstances.genBinding()},
    etna::Binding {3, drawIndirectInfo.genBinding()},
  });

  VkDescriptorSet vkSet = set.getVkSet();

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullingComputePipeline.getVkPipeline());
  
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
    m_cullingComputePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

  VkBufferMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
    .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
    .buffer = drawIndirectInfo.get(),
    .offset = 0,
    .size = VK_WHOLE_SIZE,
  };

  vkCmdPushConstants(a_cmdBuff, m_cullingComputePipeline.getVkPipelineLayout(), 
    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstCulling), &pushConstCulling);

  vkCmdDispatch(a_cmdBuff, m_groupCountX / 128u + 1u, m_pScnMgr->MeshesNum(), 1u);

  vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 
    0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void DeferredRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  VkDeviceSize offset = 0u;
  pushConst.projView = a_wvp;
  pushConst.camPos = m_cam.pos;
  for (uint32_t i = 0; i < m_pScnMgr->MeshesNum(); ++i)
  {
    pushConst.id = i;
    vkCmdPushConstants(a_cmdBuff, m_gBufferPipeline.getVkPipelineLayout(),
      stages, 0, sizeof(pushConst), &pushConst);

    vkCmdDrawIndexedIndirect(a_cmdBuff, drawIndirectInfo.get(), offset, 1, 0);
    offset += sizeof(VkDrawIndexedIndirectCommand);
  }
}

void DeferredRender::DrawFrameCmd(VkCommandBuffer a_cmdBuff) {
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_FRAGMENT_BIT);

  vkCmdPushConstants(a_cmdBuff, m_finalPassPipeline.getVkPipelineLayout(), 
    stageFlags, 0, sizeof(pushConst2M1V), &pushConst2M1V);

  vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
}

void DeferredRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  CullSceneCmd(a_cmdBuff);

  //// draw scene to gBuffer
  //
  {
    auto gBufferInfo = etna::get_shader_program("gBuffer");

    auto set = etna::create_descriptor_set(gBufferInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, instanceMatrices.genBinding()},
      etna::Binding {1, visibleInstances.genBinding()},
      etna::Binding {2, meshInfo.genBinding()},
    });


    auto bunnyTextureSet = etna::create_descriptor_set(gBufferInfo.getDescriptorLayoutId(1), a_cmdBuff, 
    {
      etna::Binding {0, bunnyAlbedoTexture.genBinding()},
      etna::Binding {1, bunnyNormalTexture.genBinding()},
      etna::Binding {2, bunnyRoughnessTexture.genBinding()},  
    });

    auto teapotTextureSet = etna::create_descriptor_set(gBufferInfo.getDescriptorLayoutId(2), a_cmdBuff, 
    {
      etna::Binding {0, teapotAlbedoTexture.genBinding()},
      etna::Binding {1, teapotNormalTexture.genBinding()},
      etna::Binding {2, teapotMetalnessTexture.genBinding()},
      etna::Binding {3, teapotRoughnessTexture.genBinding()},  
    });

    auto flatTextureSet = etna::create_descriptor_set(gBufferInfo.getDescriptorLayoutId(3), a_cmdBuff, 
    {
      etna::Binding {0, flatAlbedoTexture.genBinding()},
      etna::Binding {1, flatNormalTexture.genBinding()},
      etna::Binding {2, flatRoughnessTexture.genBinding()},  
    });

    std::vector<VkDescriptorSet> vkSet = {set.getVkSet(), bunnyTextureSet.getVkSet(), teapotTextureSet.getVkSet(), flatTextureSet.getVkSet()};

    etna::RenderTargetState renderTargets{a_cmdBuff, {m_width, m_height}, {colorMap, normalMap}, mainViewDepth};

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gBufferPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_gBufferPipeline.getVkPipelineLayout(), 0, vkSet.size(), vkSet.data(), 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj);
  }
  
  {
    auto finalPassInfo = etna::get_shader_program("finalPass");

    auto set = etna::create_descriptor_set(finalPassInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, colorMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {1, normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

    VkDescriptorSet vkSet = set.getVkSet();
    
    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_finalPassPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_finalPassPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawFrameCmd(a_cmdBuff);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
