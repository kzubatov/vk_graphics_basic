#include "../../utils/input_definitions.h"

#include "etna/Etna.hpp"
#include "shadowmap_render.h"

void SimpleShadowmapRender::UpdateCamera(const Camera* cams, uint32_t a_camsNumber)
{
  m_cam = cams[0];
  if(a_camsNumber >= 2)
    m_light.cam = cams[1];
  UpdateView(); 
}

void SimpleShadowmapRender::UpdateView()
{
  ///// calc camera matrix
  //
  const float aspect = float(m_width) / float(m_height);
  auto mProjFix = OpenglToVulkanProjectionMatrixFix();
  auto mProj = projectionMatrix(m_cam.fov, aspect, 0.1f, 1000.0f);
  auto mLookAt = LiteMath::lookAt(m_cam.pos, m_cam.lookAt, m_cam.up);
  auto mWorldViewProj = mProjFix * mProj * mLookAt;
  
  m_worldViewProj = mWorldViewProj;
  
  ///// calc light matrix
  //
  if(m_light.usePerspectiveM)
    mProj = perspectiveMatrix(m_light.cam.fov, 1.0f, 1.0f, m_light.lightTargetDist*2.0f);
  else
    mProj = ortoMatrix(-m_light.radius, +m_light.radius, -m_light.radius, +m_light.radius, 0.0f, m_light.lightTargetDist);

  if(m_light.usePerspectiveM)  // don't understang why fix is not needed for perspective case for shadowmap ... it works for common rendering  
    mProjFix = LiteMath::float4x4();
  else
    mProjFix = OpenglToVulkanProjectionMatrixFix(); 
  
  mLookAt       = LiteMath::lookAt(m_light.cam.pos, m_light.cam.pos + m_light.cam.forward()*10.0f, m_light.cam.up);
  m_lightMatrix = mProjFix*mProj*mLookAt;
}

void SimpleShadowmapRender::UpdateUniformBuffer(float a_time)
{
  m_uniforms.lightMatrix = m_lightMatrix;
  m_uniforms.lightPos    = m_light.cam.pos; //LiteMath::float3(sinf(a_time), 1.0f, cosf(a_time));
  m_uniforms.time        = a_time;

  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
}

void SimpleShadowmapRender::UpdateJitterBuffer()
{
  auto HaltonOffset = (2.0f * HaltonSequence[HaltonCounter++] - float2(1.f)) / float2(m_width, m_height);
  HaltonCounter &= 7;

  memcpy(m_jitterMappedMem, &HaltonOffset, sizeof(HaltonOffset));
}

void SimpleShadowmapRender::RecreateResolvePassResources() 
{
  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
  if (m_AAType == TAA) dynamicStates.push_back(vk::DynamicState::eStencilReference);

  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline(m_AAType == TAA ? "simple_material_TAA" : "simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .multisampleConfig = 
        {
          .rasterizationSamples = m_AAType == MSAA ? vk::SampleCountFlagBits::e4 : vk::SampleCountFlagBits::e1,
          .sampleShadingEnable = m_AAType == MSAA,
          .minSampleShading = float(m_AAType == MSAA),
        },
      .depthConfig = 
        {
          .depthTestEnable = true,
          .depthWriteEnable = true,
          .depthCompareOp = vk::CompareOp::eLessOrEqual,
          .stencilTestEnable = m_AAType == TAA,
          .front = {vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 1, 1, 0},
          .back = {vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 1, 1, 0},
          .maxDepthBounds = 1.f,
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = m_AAType == TAA ? vk::Format::eD32SfloatS8Uint : vk::Format::eD32Sfloat,
          .stencilAttachmentFormat = m_AAType == TAA ? vk::Format::eD32SfloatS8Uint : vk::Format::eUndefined,
        },
      .dynamicStates = dynamicStates, 
    });

  switch (m_AAType)
  {
  case NoAA:
    AAimage.reset();
    TAAHistoryBuffer.reset();
    velocityBuffer.reset();
    mainViewDepth = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "main_view_depth",
      .format = vk::Format::eD32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
    });
    break;
  case MSAA:
    TAAHistoryBuffer.reset();
    velocityBuffer.reset();
    AAimage = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "AA_image",
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
      .samples = vk::SampleCountFlagBits::e4,
    });
    mainViewDepth = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "main_view_depth",
      .format = vk::Format::eD32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .samples = vk::SampleCountFlagBits::e4,
    });
    break;
  case SSAA:
    TAAHistoryBuffer.reset();
    velocityBuffer.reset();
    AAimage = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width << 1u, m_height << 1u, 1u},
      .name = "AA_image",
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    });
    mainViewDepth = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width << 1u, m_height << 1u, 1u},
      .name = "main_view_depth",
      .format = vk::Format::eD32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
    });
    m_resolvePipeline = pipelineManager.createGraphicsPipeline("resolve_pass",
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
        },
    });
    break;
  case TAA:
    m_prevWorldViewProj = m_worldViewProj;
    m_prevModelMatrix = translate4x4({0.f, 0.2f * sinf(4.0f * m_uniforms.time), 0.f}) * m_pScnMgr->GetInstanceMatrix(5);

    TAAHistoryBuffer = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "TAA_history_buffer",
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    });
    velocityBuffer = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "velocity_buffer",
      .format = vk::Format::eR16G16Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    });
    AAimage = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "AA_image",
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    });
    mainViewDepth = m_context->createImage(etna::Image::CreateInfo
    {
      .extent = vk::Extent3D{m_width, m_height, 1u},
      .name = "main_view_depth",
      .format = vk::Format::eD32SfloatS8Uint,
      .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | 
                    vk::ImageUsageFlagBits::eSampled,
    });

    m_resolvePipeline = pipelineManager.createGraphicsPipeline("resolve_pass_TAA",
    {
      .vertexShaderInput = {},
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
        },
    });
  default:
    break;
  }
}

void SimpleShadowmapRender::ProcessInput(const AppInput &input)
{
  // add keyboard controls here
  // camera movement is processed separately
  //
  if(input.keyReleased[GLFW_KEY_Q])
    m_input.drawFSQuad = !m_input.drawFSQuad;

  if(input.keyReleased[GLFW_KEY_P])
    m_light.usePerspectiveM = !m_light.usePerspectiveM;

  // recreate pipeline to reload shaders
  if(input.keyPressed[GLFW_KEY_B])
  {
#ifdef WIN32
    std::system("cd ../resources/shaders && python compile_shadowmap_shaders.py");
#else
    std::system("cd ../resources/shaders && python3 compile_shadowmap_shaders.py");
#endif

    etna::reload_shaders();

    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_swapchain.GetAttachment(i).image, m_swapchain.GetAttachment(i).view);
    }
  }
}
