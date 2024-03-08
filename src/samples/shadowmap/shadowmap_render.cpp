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
    .extent = vk::Extent3D{m_width, m_height, 1u},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048u, 2048u, 1u},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  linearSampler = etna::Sampler(etna::Sampler::CreateInfo
  {
    .filter = vk::Filter::eLinear,
    .name = "linear_sampler",
  });
  
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  m_uboMappedMem = constants.map();

  jitter = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(float2),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_COPY,
    .name = "jitter",
  });

  m_jitterMappedMem = jitter.map();
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  m_swapchain.Cleanup();
  AAimage.reset();
  velocityBuffer.reset();
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
  etna::create_program("simple_material", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", 
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
  });

  etna::create_program("simple_material_TAA", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", 
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_jitter.vert.spv"
  });

  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  
  etna::create_program("resolve_pass", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad.frag.spv",
  });

  etna::create_program("velocity_pass", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/velocity.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/velocity.frag.spv",
  });

  etna::create_program("resolve_pass_TAA", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/taa.frag.spv",
  });
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

  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        },
    });

  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        },
    });

  m_velocityPipeline = pipelineManager.createGraphicsPipeline("velocity_pass",
  {
    .vertexShaderInput = sceneVertexInputDesc,
    .depthConfig =
    {
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .stencilTestEnable = true,
      .front = {vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eEqual, 1, 1, 1},
      .back = {vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eEqual, 1, 1, 1},
    },
    .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {vk::Format::eR16G16Sfloat},
        .stencilAttachmentFormat = vk::Format::eD32SfloatS8Uint,
      },
  });
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    if (m_useStencil)
      vkCmdSetStencilReference(a_cmdBuff, VK_STENCIL_FACE_FRONT_AND_BACK, i == 5);

    auto inst         = m_pScnMgr->GetInstanceInfo(i);
    
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
    if (i == 5)
      pushConst2M.model = translate4x4(float3(0.f, 0.2f * sinf(4.0f * m_uniforms.time), 0.f)) * pushConst2M.model; 

    vkCmdPushConstants(a_cmdBuff, a_pipelineLayout,
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  if (m_clearHistoryBuffer)
  {
    etna::set_state(a_cmdBuff, TAAHistoryBuffer.get(), vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
      vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor);

    etna::flush_barriers(a_cmdBuff);

    VkClearColorValue clearColorValue = {0, 0, 0, 0};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(a_cmdBuff, TAAHistoryBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &range);

    m_clearHistoryBuffer = false;
  }

  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048}, {}, shadowMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());

    m_useStencil = false;
    DrawSceneCmd(a_cmdBuff, m_lightMatrix, m_shadowPipeline.getVkPipelineLayout());
  }

  //// draw final scene to screen
  //
  {
    auto simpleMaterialInfo = etna::get_shader_program(m_AAType == AA::TAA ? "simple_material_TAA" : "simple_material");

    std::vector<etna::Binding> bindings = 
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    };

    if (m_AAType == AA::TAA) bindings.push_back({2, jitter.genBinding()});

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), a_cmdBuff, bindings);

    VkDescriptorSet vkSet = set.getVkSet();

    vk::Rect2D rect {0, 0, m_width, m_height};
    std::vector<etna::RenderTargetState::AttachmentParams> colorAttachments {};
    etna::RenderTargetState::AttachmentParams stencilAttachment {};

    switch (m_AAType)
    {
    case AA::SSAA:
      rect.extent.width <<= 1u;
      rect.extent.height <<= 1u;
    case AA::MSAA:
      colorAttachments.push_back(AAimage);
      break;
    case AA::TAA:
      stencilAttachment = {mainViewDepth};
      colorAttachments.push_back(AAimage);
      break;
    default:
      colorAttachments.push_back({a_targetImage, a_targetImageView});
      break;
    }

    etna::RenderTargetState renderTargets(a_cmdBuff, rect, colorAttachments, {mainViewDepth}, stencilAttachment);
    
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    m_useStencil = m_AAType == AA::TAA;
    DrawSceneCmd(a_cmdBuff, m_worldViewProj, m_basicForwardPipeline.getVkPipelineLayout());
  }

  //// resolve pass for AA
  //
  if (m_AAType == AA::MSAA) 
  {
    etna::set_state(a_cmdBuff, AAimage.get(), vk::PipelineStageFlagBits2::eResolve,
      vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eTransferSrcOptimal,
      vk::ImageAspectFlagBits::eColor);

    etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eResolve,
      vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal,
      vk::ImageAspectFlagBits::eColor);
      
    etna::flush_barriers(a_cmdBuff);

    VkImageResolve imageResolve = {
      .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .srcOffset = {0, 0, 0},
      .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .dstOffset = {0, 0, 0},
      .extent = {m_width, m_height, 1},
    };

    vkCmdResolveImage(a_cmdBuff, AAimage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
      a_targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageResolve);
    
    etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor);
    
    etna::flush_barriers(a_cmdBuff);
  }
  else if (m_AAType == AA::SSAA)
  {
    auto resolveInfo = etna::get_shader_program("resolve_pass");

    auto set = etna::create_descriptor_set(resolveInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, AAimage.genBinding(linearSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height}, {{a_targetImage, a_targetImageView}}, {});
    
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_resolvePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
  }
  else if (m_AAType == AA::TAA)
  {
    //// velocity pass
    //
    {
      etna::RenderTargetState::AttachmentParams stencilAttachment {mainViewDepth};
      stencilAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
      etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height}, {velocityBuffer}, {}, stencilAttachment);
      
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_velocityPipeline.getVkPipeline());

      VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

      VkDeviceSize zero_offset = 0u;
      VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
      VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
      
      vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
      vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);
        
      pushConst2M.projView = m_prevWorldViewProj * m_prevModelMatrix; // prev
      m_prevModelMatrix = translate4x4({0.f, 0.2f * sinf(4.0f * m_uniforms.time), 0.f}) * m_pScnMgr->GetInstanceMatrix(5); // very good place for prev matrix update
      pushConst2M.model = m_worldViewProj * m_prevModelMatrix; // current

      vkCmdPushConstants(a_cmdBuff, m_velocityPipeline.getVkPipelineLayout(),
        stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

      auto inst = m_pScnMgr->GetInstanceInfo(5);
      auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
      vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
    }

    //// resolve pass
    //
    {
      auto resolveInfo = etna::get_shader_program("resolve_pass_TAA");

      auto set = etna::create_descriptor_set(resolveInfo.getDescriptorLayoutId(0), a_cmdBuff,
      {
        etna::Binding {0, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {0, 1, vk::ImageAspectFlagBits::eDepth})},
        etna::Binding {1, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {0, 1, vk::ImageAspectFlagBits::eStencil})},
        etna::Binding {2, velocityBuffer.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding {3, TAAHistoryBuffer.genBinding(linearSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding {4, AAimage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      });

      VkDescriptorSet vkSet = set.getVkSet();

      etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height}, {{a_targetImage, a_targetImageView}}, {});
      
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipeline.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_resolvePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
      
      struct { float4x4 mx; float2 res; } pushConst = {m_prevWorldViewProj * inverse4x4(m_worldViewProj), float2(m_width, m_height)};
      vkCmdPushConstants(a_cmdBuff, m_resolvePipeline.getVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConst), &pushConst);

      vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);

      m_prevWorldViewProj = m_worldViewProj; // prev wvp update 
    }

    {
      etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor);
      etna::set_state(a_cmdBuff, TAAHistoryBuffer.get(), vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor);
      
      etna::flush_barriers(a_cmdBuff);

      VkImageCopy2 region =
      {
        VK_STRUCTURE_TYPE_IMAGE_COPY_2,
        nullptr,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {0, 0, 0},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {0, 0, 0},
        {m_width, m_height, 1},
      };

      VkCopyImageInfo2 copyImageInfo = 
      {
        VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
        nullptr,
        a_targetImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        TAAHistoryBuffer.get(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region,
      };

      vkCmdCopyImage2(a_cmdBuff, &copyImageInfo);

      etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(a_cmdBuff);
    }
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
