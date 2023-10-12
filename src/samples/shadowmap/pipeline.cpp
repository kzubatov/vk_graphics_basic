#include "shadowmap_render.h"
#include <vk_pipeline.h>

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             14},
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 6);
  
  auto shadowMap = m_pShadowMap2->m_attachments[m_shadowMapId];

  m_dSet.resize(5);
  m_dSetLayout.resize(5);

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindImage (1, shadowMap.view, m_pShadowMap2->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_dSet[0], &m_dSetLayout[0]);

  m_pBindings->BindBegin(VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_matrixBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(1, m_shadowMapVisibleInstancesBuffer, VK_NULL_HANDLE);
  m_pBindings->BindEnd(&m_dSet[1], &m_dSetLayout[1]);

  m_pBindings->BindBegin(VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_matrixBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(1, m_forwardVisibleInstancesBuffer, VK_NULL_HANDLE);
  m_pBindings->BindEnd(&m_dSet[2], &m_dSetLayout[2]);

  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_matrixBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(1, m_boundingSpheresBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(2, m_forwardVisibleInstancesBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(3, m_forwardDrawIndirectCmdBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(4, m_meshInstancesCountBuffer, VK_NULL_HANDLE);
  m_pBindings->BindEnd(&m_dSet[3], &m_dSetLayout[3]);

  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_matrixBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(1, m_boundingSpheresBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(2, m_shadowMapVisibleInstancesBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(3, m_shadowMapDrawIndirectCmdBuffer, VK_NULL_HANDLE);
  m_pBindings->BindBuffer(4, m_meshInstancesCountBuffer, VK_NULL_HANDLE);
  m_pBindings->BindEnd(&m_dSet[4], &m_dSetLayout[4]);

  //m_pBindings->BindImage(0, m_GBufTarget->m_attachments[m_GBuf_idx[GBUF_ATTACHMENT::POS_Z]].view, m_GBufTarget->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.view, m_pShadowMap2->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  // if we are recreating pipeline (for example, to reload shaders)
  // we need to cleanup old pipeline
  if (m_basicForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_basicForwardPipeline.layout, nullptr);
    m_basicForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if (m_basicForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_basicForwardPipeline.pipeline, nullptr);
    m_basicForwardPipeline.pipeline = VK_NULL_HANDLE;
  }

  if (m_shadowPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_shadowPipeline.pipeline, nullptr);
    m_shadowPipeline.pipeline = VK_NULL_HANDLE;
  }

  if (m_cullingPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_cullingPipeline.pipeline, nullptr);
    m_cullingPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_cullingPipeline.layout != VK_NULL_HANDLE) 
  {
    vkDestroyPipelineLayout(m_device, m_cullingPipeline.layout, nullptr);
    m_cullingPipeline.layout = VK_NULL_HANDLE;
  }

  vk_utils::GraphicsPipelineMaker graphicsMaker;
  vk_utils::ComputePipelineMaker computeMaker;
  
  // pipeline for drawing objects
  //
  std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
  {
    shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = "../resources/shaders/simple_shadow.frag.spv";
    shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = "../resources/shaders/simple.vert.spv";
  }
  graphicsMaker.LoadShaders(m_device, shader_paths);

  m_basicForwardPipeline.layout = graphicsMaker.MakeLayout(m_device, {m_dSetLayout[0], m_dSetLayout[2]} , sizeof(pushConst));
  graphicsMaker.SetDefaultState(m_width, m_height);
  
  m_basicForwardPipeline.pipeline = graphicsMaker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                               m_screenRenderPass);

  // pipeline for rendering objects to shadowmap
  //
  shader_paths.clear();
  shader_paths[VK_SHADER_STAGE_VERTEX_BIT] = "../resources/shaders/simple.vert.spv";
  graphicsMaker.LoadShaders(m_device, shader_paths);
  graphicsMaker.viewport.width  = float(m_pShadowMap2->m_resolution.width);
  graphicsMaker.viewport.height = float(m_pShadowMap2->m_resolution.height);
  graphicsMaker.scissor.extent  = VkExtent2D{ uint32_t(m_pShadowMap2->m_resolution.width), uint32_t(m_pShadowMap2->m_resolution.height) };

  m_shadowPipeline.layout   = m_basicForwardPipeline.layout;
  m_shadowPipeline.pipeline = graphicsMaker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(), 
                                                 m_pShadowMap2->m_renderPass);

  // pipeline for culling objects
  // 
  computeMaker.LoadShader(m_device, "../resources/shaders/culling.comp.spv");

  m_cullingPipeline.layout = computeMaker.MakeLayout(m_device, {m_dSetLayout[3]}, sizeof(cullPushConst));
  m_cullingPipeline.pipeline = computeMaker.MakePipeline(m_device);
}