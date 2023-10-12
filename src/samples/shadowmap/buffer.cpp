#include "shadowmap_render.h"

#include <vk_buffers.h>

void SimpleShadowmapRender::CreateBuffer(VkBuffer &buffer, VkDeviceMemory &memory, 
                                         VkDeviceSize size, VkBufferUsageFlags bufFlags,
                                         VkMemoryPropertyFlags memFlags)
{
  VkMemoryRequirements memReq;

  buffer = vk_utils::createBuffer(m_device, size, bufFlags, &memReq);
  
  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.pNext = nullptr;
  allocateInfo.allocationSize = memReq.size;
  allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits, memFlags, m_physicalDevice);
  
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &memory));
  VK_CHECK_RESULT(vkBindBufferMemory(m_device, buffer, memory, 0));
}

void SimpleShadowmapRender::CreateBuffer(VkBuffer &buffer, VkDeviceMemory &memory,
                                         void **map, VkDeviceSize size, VkBufferUsageFlags bufFlags,
                                         VkMemoryPropertyFlags memFlags)
{
  CreateBuffer(buffer, memory, size, bufFlags, memFlags);
  vkMapMemory(m_device, memory, 0, size, 0, map);
}

void SimpleShadowmapRender::CreateMeshesBuffers()
{
  const uint32_t additionalTeapotsCount = 10000;
  auto instancesNum = m_pScnMgr->InstancesNum() + additionalTeapotsCount;
  CreateBuffer(m_matrixBuffer, m_matrixBufferAlloc, &m_matrixBufferMappedMem, sizeof(float4x4) * instancesNum, 
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

  CreateBuffer(m_boundingSpheresBuffer, m_boundingSpheresBufferAlloc, &m_boundingSpheresBufferMappedMem,
               sizeof(float4) * m_pScnMgr->MeshesNum(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

  CreateBuffer(m_meshInstancesCountBuffer, m_meshInstancesCountBufferAlloc, &m_meshInstancesCountBufferMappedMem,
               sizeof(uint32_t) * m_pScnMgr->MeshesNum(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
}

void SimpleShadowmapRender::InitMeshesBuffers() 
{
  std::vector<std::vector<float4x4>> meshModelMatrices(m_pScnMgr->MeshesNum());

  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i) 
  {
    meshModelMatrices[m_pScnMgr->GetInstanceInfo(i).mesh_id].push_back(m_pScnMgr->GetInstanceMatrix(i));
  }

  const int32_t additionalTeapotsCount = 10000;
  const uint32_t teapotMeshID = 1;
  for (int32_t i = 0; i < additionalTeapotsCount; ++i)
  {
    auto modelMatrix = m_pScnMgr->GetInstanceMatrix(teapotMeshID);
    modelMatrix.col(3).x += (i / 100 - 50) * 2;
    modelMatrix.col(3).y += (i % 100 - 50) * 2;
    meshModelMatrices[teapotMeshID].push_back(modelMatrix);
  }

  std::vector<float4> m_boundingSpheres(m_pScnMgr->MeshesNum());
  m_meshInstancesCount.resize(m_pScnMgr->MeshesNum());
  uint32_t offset = 0;
  for (uint32_t i = 0; i < m_pScnMgr->MeshesNum(); ++i)
  {
    m_meshInstancesCount[i] = meshModelMatrices[i].size();
    
    if (meshModelMatrices[i].size())
    {
      auto bBoxMin = m_pScnMgr->GetMeshBbox(i).boxMin;
      auto bBoxMax = m_pScnMgr->GetMeshBbox(i).boxMax;

      m_boundingSpheres[i] = (bBoxMin + bBoxMax) / 2.0f;
      m_boundingSpheres[i].w = LiteMath::length3(bBoxMax - bBoxMin) / 2.0f;

      uint32_t size = meshModelMatrices[i].size() * sizeof(float4x4);
      memcpy((char *) m_matrixBufferMappedMem + offset, meshModelMatrices[i].data(), size);
      offset += size;
    }
  }

  memcpy((char *) m_meshInstancesCountBufferMappedMem, m_meshInstancesCount.data(), m_meshInstancesCount.size() * sizeof(uint32_t));
  memcpy((char *) m_boundingSpheresBufferMappedMem, m_boundingSpheres.data(), m_boundingSpheres.size() * sizeof(float4));
}

void SimpleShadowmapRender::CreateUniformBuffer()
{
  VkMemoryRequirements memReq;
  m_ubo = vk_utils::createBuffer(m_device, sizeof(UniformParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &memReq);

  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.pNext = nullptr;
  allocateInfo.allocationSize = memReq.size;
  allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                          m_physicalDevice);
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_uboAlloc));
  VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_ubo, m_uboAlloc, 0));

  vkMapMemory(m_device, m_uboAlloc, 0, sizeof(m_uniforms), 0, &m_uboMappedMem);

  UpdateUniformBuffer(0.0f);
}

void SimpleShadowmapRender::UpdateUniformBuffer(float a_time)
{
  m_uniforms.lightMatrix = m_lightMatrix;
  m_uniforms.lightPos    = m_light.cam.pos; //LiteMath::float3(sinf(a_time), 1.0f, cosf(a_time));
  
  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  m_uniforms.lightColor  = LiteMath::mix(dark_violet, chartreuse, abs(sin(a_time)));

  m_uniforms.time = a_time;
  
  m_uniforms.baseColor = LiteMath::float3(0.9f, 0.92f, 1.0f);
  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
}

void SimpleShadowmapRender::CreateIndirectDrawBuffers()
{
  const uint32_t additionalTeapotsCount = 10000;
  const uint32_t instanceNum = m_pScnMgr->InstancesNum() + additionalTeapotsCount;

  CreateBuffer(m_shadowMapDrawIndirectCmdBuffer, m_shadowMapDrawIndirectCmdBufferAlloc,
               sizeof(VkDrawIndexedIndirectCommand) * m_pScnMgr->MeshesNum(),
               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CreateBuffer(m_shadowMapVisibleInstancesBuffer, m_shadowMapVisibleInstancesBufferAlloc,
               sizeof(uint32_t) * instanceNum, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CreateBuffer(m_forwardDrawIndirectCmdBuffer, m_forwardDrawIndirectCmdBufferAlloc,
               sizeof(VkDrawIndexedIndirectCommand) * m_pScnMgr->MeshesNum(),
               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CreateBuffer(m_forwardVisibleInstancesBuffer, m_forwardVisibleInstancesBufferAlloc,
               sizeof(uint32_t) * instanceNum, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}