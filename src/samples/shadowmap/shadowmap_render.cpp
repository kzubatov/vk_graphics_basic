#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  heightMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_heightMapInfo.pushConst.width, m_heightMapInfo.pushConst.height, 1},
    .name = "height_map",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  tessellationConstants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(TessellationParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "tess_params",
  });

  m_uboMappedMem = constants.map();
  m_tessMappedMem = tessellationConstants.map();
}

void SimpleShadowmapRender::LoadScene(const char*, bool)
{
  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();
  
  m_cam.fov = 45.0f;
  m_cam.pos = float3(10.f, 5.f, 0.f);
  m_cam.up  = float3(0.f, 1.f, 0.f);
  m_cam.lookAt = float3(0.f, 0.f, 0.f);
  m_cam.tdist  = 100.f;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  m_swapchain.Cleanup();
  heightMap.reset();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
  tessellationConstants = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pQuad = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{ 
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .rect = { 0, 0, 512, 512 }, 
    });
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material_wireframe", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.tesc.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.tese.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/wireframe.geom.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_wireframe.frag.spv",
    // VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv",
  });
  etna::create_program("simple_material", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.tesc.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.tese.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain.frag.spv",
    // VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv",
  });
  etna::create_program("height_from_noise", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/noise.frag.spv"
  });
  etna::create_program("simple_shadow", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.tesc.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/terrain_grid.tese.spv",
  });
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline(
    drawWireframe ? "simple_material_wireframe" : "simple_material",
    {
      .vertexShaderInput = {},
      .inputAssemblyConfig = 
        {
          .topology = vk::PrimitiveTopology::ePatchList,
        },
      .tessellationConfig =
        {
          .patchControlPoints = 4,
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = {},
      .inputAssemblyConfig = 
        {
          .topology = vk::PrimitiveTopology::ePatchList,
        },
      .tessellationConfig =
        {
          .patchControlPoints = 4,
        },
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_heightPipeline = pipelineManager.createGraphicsPipeline("height_from_noise",
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR8G8B8A8Unorm},
        }
    });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  vkCmdPushConstants(a_cmdBuff, a_pipelineLayout, stageFlags, 0, sizeof(a_wvp), &a_wvp);

  vkCmdDraw(a_cmdBuff, 4, m_tessParams.sqrtPatchCount * m_tessParams.sqrtPatchCount, 0, 0);
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  //// draw heightmap
  //
  if (m_heightMapInfo.recreate)
  {
    etna::RenderTargetState renderTarget(a_cmdBuff,
      {0, 0, m_heightMapInfo.pushConst.width, m_heightMapInfo.pushConst.width},
      {{.image = heightMap.get(), .view = heightMap.getView({})}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_heightPipeline.getVkPipeline());
    vkCmdPushConstants(a_cmdBuff, m_heightPipeline.getVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(m_heightMapInfo.pushConst), &m_heightMapInfo.pushConst);
    vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);

    m_heightMapInfo.recreate = false;
  }

  //// draw scene to shadowmap
  //
  {
    auto simpleShadowInfo = etna::get_shader_program("simple_shadow");
    auto set = etna::create_descriptor_set(simpleShadowInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, heightMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {1, tessellationConstants.genBinding()},
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048}, {}, {.image = shadowMap.get(), .view = shadowMap.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipelineLayout(),
      0, 1, &vkSet, 0, nullptr);

    DrawSceneCmd(a_cmdBuff, m_lightWorldViewProj, m_shadowPipeline.getVkPipelineLayout());
  }

  //// draw final scene to screen
  //
  {
    auto simpleMaterialInfo = etna::get_shader_program(drawWireframe ? "simple_material_wireframe" : "simple_material");

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, heightMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {1, tessellationConstants.genBinding()},
      etna::Binding {2, constants.genBinding()},
      etna::Binding {3, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height},
      {{.image = a_targetImage, .view = a_targetImageView}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj, m_basicForwardPipeline.getVkPipelineLayout());
  }

  if(m_input.drawFSQuad)
    m_pQuad->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, shadowMap, defaultSampler);

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
