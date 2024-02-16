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
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  m_heightPass.texture = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_heightPass.width, m_heightPass.height, 1},
    .name = "height_map",
    .format = m_heightPass.format,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
  });

  fogMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width / 4, m_height / 4, 1},
    .name = "fog_map",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  m_uboMappedMem = constants.map();
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  m_cam.fov = 40.f;
  m_cam.pos = float3(40.f, 20.f, 0.f);
  m_cam.up  = float3(0.f, 1.f, 0.f);
  m_cam.lookAt = float3(0.f, 10.f, 0.f);
  m_cam.tdist  = 100.f;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  m_heightPass.texture.reset();
  m_swapchain.Cleanup();
  fogMap.reset();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, 512, 512);
  m_pFSQuad->Create(m_context->getDevice(),
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad.frag.spv",
    vk_utils::RenderTargetInfo2D{
      .size          = VkExtent2D{ m_width, m_height },// this is debug full screen quad
      .format        = m_swapchain.GetFormat(),
      .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,// seems we need LOAD_OP_LOAD if we want to draw quad to part of screen
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    }
  );
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad_template.vert.spv",
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv"
    }
  );
  etna::create_program("simple_shadow", 
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad_template.vert.spv", 
    }
  );
  etna::create_program("height_map", 
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/noise.frag.spv"
    }
  );
  etna::create_program("fog",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/fog.frag.spv"
    }
  );
  etna::create_program("blending",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad.frag.spv"
    }
  );
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_context->getDevice(), dtypes, 2);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.getView({}), defaultSampler.get(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);


  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        },
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        },
    });
  m_heightPass.pipeline = pipelineManager.createGraphicsPipeline("height_map", 
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput = 
        {
          .colorAttachmentFormats = {m_heightPass.format},
        },
    });
  m_postFXPipeline = pipelineManager.createGraphicsPipeline("fog", 
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput = 
        {
          .colorAttachmentFormats = {vk::Format::eR8G8B8A8Unorm},
        } ,   
    });
  m_finalPassPipeline = pipelineManager.createGraphicsPipeline("blending",
    {
      .vertexShaderInput = {},
      .blendingConfig = 
        {
          .attachments = 
            {
              vk::PipelineColorBlendAttachmentState 
              {
                .blendEnable = true,
                .srcColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                .dstColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                .colorBlendOp = vk::BlendOp::eAdd,
                .alphaBlendOp = vk::BlendOp::eMax,
                .colorWriteMask = vk::ColorComponentFlagBits::eR
                  | vk::ColorComponentFlagBits::eG
                  | vk::ColorComponentFlagBits::eB
                  | vk::ColorComponentFlagBits::eA,
              }
            },
          .logicOpEnable = false,
          .logicOp = vk::LogicOp::eNoOp,
          .blendConstants = {0, 0, 0, 0},
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
        },
    });
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}

/// COMMAND BUFFER FILLING
void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  if (m_heightPass.recreate)
  {
    m_heightPass.recreate = false;
    
    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_heightPass.width, m_heightPass.height}, {m_heightPass.texture}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_heightPass.pipeline.getVkPipeline());
    vkCmdPushConstants(a_cmdBuff, m_heightPass.pipeline.getVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, 
      sizeof(m_heightPass.pushConst), &m_heightPass.pushConst);
    vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
  }

  //// draw scene to shadowmap
  //
  {
    auto info = etna::get_shader_program("simple_shadow");

    auto set = etna::create_descriptor_set(info.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, m_heightPass.texture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048}, {}, shadowMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_shadowPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    pushConstQuad.projView = m_lightMatrix;
    vkCmdPushConstants(a_cmdBuff, m_shadowPipeline.getVkPipelineLayout(), 
      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstQuad), &pushConstQuad);
    
    vkCmdDraw(a_cmdBuff, 6, pushConstQuad.tes_level * pushConstQuad.tes_level, 0, 0);
  }

  //// draw scene
  //
  {
    auto info = etna::get_shader_program("simple_material");

    auto set0 = etna::create_descriptor_set(info.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, m_heightPass.texture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    auto set1 = etna::create_descriptor_set(info.getDescriptorLayoutId(1), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    std::array<VkDescriptorSet, 2> vkSet = {set0.getVkSet(), set1.getVkSet()};

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height}, {{a_targetImage, a_targetImageView}}, mainViewDepth);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 2, vkSet.data(), 0, VK_NULL_HANDLE);

    pushConstQuad.projView = m_worldViewProj;
    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(), 
      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstQuad), &pushConstQuad);

    vkCmdDraw(a_cmdBuff, 6, pushConstQuad.tes_level * pushConstQuad.tes_level, 0, 0);
  }

  //// draw fog
  //
  {
    auto info = etna::get_shader_program("fog");

    auto set = etna::create_descriptor_set(info.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width >> 2u, m_height >> 2u}, {fogMap}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postFXPipeline.getVkPipeline());

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_postFXPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    
    vkCmdPushConstants(a_cmdBuff, m_postFXPipeline.getVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConst2M), &pushConst2M);
    
    vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
  }

  //// blend maps
  //
  {
    auto info = etna::get_shader_program("blending");

    auto set = etna::create_descriptor_set(info.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, fogMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0,  m_width, m_height}, {{a_targetImage, a_targetImageView, false}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_finalPassPipeline.getVkPipeline());

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_finalPassPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    
    vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
  }

  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
