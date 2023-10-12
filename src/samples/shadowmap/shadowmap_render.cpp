#include "shadowmap_render.h"

// #include <geom/vk_mesh.h>

void SimpleShadowmapRender::CullSceneCmd(VkCommandBuffer a_cmdBuff, uint32_t a_cameraId)
{
  VkBuffer buf = a_cameraId ? m_shadowMapDrawIndirectCmdBuffer : m_forwardDrawIndirectCmdBuffer;
  VkDeviceSize offset = 0;

  for (uint32_t i = 0; i < m_pScnMgr->MeshesNum(); ++i) {
    auto mesh_info = m_pScnMgr->GetMeshInfo(i);
    vkCmdFillBuffer(a_cmdBuff, buf, offset, sizeof(uint32_t), mesh_info.m_indNum);
    vkCmdFillBuffer(a_cmdBuff, buf, offset + sizeof(uint32_t), sizeof(uint32_t), 0);
    vkCmdFillBuffer(a_cmdBuff, buf, offset + 2 * sizeof(uint32_t), sizeof(uint32_t), mesh_info.m_indexOffset);
    vkCmdFillBuffer(a_cmdBuff, buf, offset + 3 * sizeof(uint32_t), sizeof(uint32_t), mesh_info.m_vertexOffset);
    vkCmdFillBuffer(a_cmdBuff, buf, offset + 4 * sizeof(uint32_t), sizeof(uint32_t), 0);
    offset += sizeof(VkDrawIndexedIndirectCommand);
  }
  
  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullingPipeline.pipeline);
  
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
    m_cullingPipeline.layout, 0, 1, &m_dSet[3 + a_cameraId], 0, VK_NULL_HANDLE);

  VkBufferMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .buffer = buf,
    .offset = 0,
    .size = VK_WHOLE_SIZE,
  };

  cullPushConst.meshesCount = m_pScnMgr->MeshesNum();
  cullPushConst.frustum = m_frustum[a_cameraId];

  vkCmdPushConstants(a_cmdBuff, m_cullingPipeline.layout, 
    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(cullPushConst), &cullPushConst);

  vkCmdDispatch(a_cmdBuff, (m_pScnMgr->InstancesNum() + 10000) / 128u + 1u, 1u, 1u);

  vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 
    0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, uint32_t a_cameraId)
{
  VkBuffer buf = a_cameraId ? m_shadowMapDrawIndirectCmdBuffer : m_forwardDrawIndirectCmdBuffer;

  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst.projView = a_wvp;
  uint32_t offset = 0;
  for (uint32_t i = 0; i < m_pScnMgr->MeshesNum(); ++i)
  {
    if (m_meshInstancesCount[i])
    {
      pushConst.offset = offset;
      vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.layout, stageFlags, 0, sizeof(pushConst), &pushConst);

      vkCmdDrawIndexedIndirect(a_cmdBuff, buf, i * sizeof(VkDrawIndexedIndirectCommand), 1, 0);
      offset += m_meshInstancesCount[i];
    }
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                                     VkImageView a_targetImageView, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  VkViewport viewport{};
  VkRect2D scissor{};
  VkExtent2D ext;
  ext.height = m_height;
  ext.width  = m_width;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width  = static_cast<float>(ext.width);
  viewport.height = static_cast<float>(ext.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  scissor.offset = {0, 0};
  scissor.extent = ext;

  std::vector<VkViewport> viewports = {viewport};
  std::vector<VkRect2D> scissors = {scissor};
  vkCmdSetViewport(a_cmdBuff, 0, 1, viewports.data());
  vkCmdSetScissor(a_cmdBuff, 0, 1, scissors.data());

  CullSceneCmd(a_cmdBuff, 1);
  CullSceneCmd(a_cmdBuff, 0);

  //// draw scene to shadowmap
  //
  {
    VkClearValue clearDepth = {};
    clearDepth.depthStencil.depth   = 1.0f;
    clearDepth.depthStencil.stencil = 0;
  
    std::vector<VkClearValue> clear =  {clearDepth};
    VkRenderPassBeginInfo renderToShadowMap = m_pShadowMap2->GetRenderPassBeginInfo(0, clear);
  
    vkCmdBeginRenderPass(a_cmdBuff, &renderToShadowMap, VK_SUBPASS_CONTENTS_INLINE);
  
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.pipeline);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.layout, 1, 1, &m_dSet[1], 0, VK_NULL_HANDLE);
    DrawSceneCmd(a_cmdBuff, m_lightMatrix, 1);
  
    vkCmdEndRenderPass(a_cmdBuff);
  }

  //// draw final scene to screen
  //
  {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_screenRenderPass;
    renderPassInfo.framebuffer = a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.GetExtent();

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_pipeline);
    VkDescriptorSet sets[] = {m_dSet[0], m_dSet[2]};
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.layout, 0, 2, sets, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj, 0);

    vkCmdEndRenderPass(a_cmdBuff);
  }

  if (m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}