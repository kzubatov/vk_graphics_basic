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

  ///// frustum planes
  //
  pushConstCulling[0].bBoxMin = pushConstCulling[1].bBoxMin = m_pScnMgr->GetMeshBbox(0).boxMin;
  pushConstCulling[0].bBoxMax = pushConstCulling[1].bBoxMax = m_pScnMgr->GetMeshBbox(0).boxMax; 
  pushConstCulling[0].count = pushConstCulling[1].count = grid_n * grid_n;
  {
    // light camera
    auto pos = m_light.cam.pos;
    auto forward = m_light.cam.forward();
    auto up = LiteMath::normalize(m_light.cam.up);
    auto right = m_light.cam.right();
    
    float zNear = 1.0f;
    float zFar = m_light.lightTargetDist * 2.0f;
    float t_aspect = 1.0f;
    
    float halfFrustumHeight = zFar * tanf(m_light.cam.fov * LiteMath::DEG_TO_RAD * 0.5f);
    float halfFrustumWidth = halfFrustumHeight * t_aspect;

    auto nearNorm = forward;
    pushConstCulling[0].plane[0] = LiteMath::to_float4(nearNorm, LiteMath::dot(nearNorm, forward * zNear + pos));

    auto farNorm = forward * -1.0f;
    pushConstCulling[0].plane[1] = LiteMath::to_float4(farNorm, LiteMath::dot(farNorm, forward * zFar + pos));

    auto topNorm = LiteMath::normalize(LiteMath::cross(right, forward * zFar - up * halfFrustumHeight));
    pushConstCulling[0].plane[2] = LiteMath::to_float4(topNorm, LiteMath::dot(topNorm, pos));

    auto bottomNorm = LiteMath::normalize(LiteMath::cross(forward * zFar + up * halfFrustumHeight, right));
    pushConstCulling[0].plane[3] = LiteMath::to_float4(bottomNorm, LiteMath::dot(bottomNorm, pos));

    auto rightNorm = LiteMath::normalize(LiteMath::cross(forward * zFar - right * halfFrustumWidth, up));
    pushConstCulling[0].plane[4] = LiteMath::to_float4(rightNorm, LiteMath::dot(rightNorm, pos));

    auto leftNorm = LiteMath::normalize(LiteMath::cross(up, forward * zFar + right * halfFrustumWidth));
    pushConstCulling[0].plane[5] = LiteMath::to_float4(leftNorm, LiteMath::dot(leftNorm, pos));
  }

  {
    // camera
    auto pos = m_cam.pos;
    auto forward = m_cam.forward();
    auto up = LiteMath::normalize(m_cam.up);
    auto right = m_cam.right();
    
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float t_aspect = float(m_width) / float(m_height);
    
    float halfFrustumHeight = zFar * tanf(m_cam.fov * LiteMath::DEG_TO_RAD * 0.5f);
    float halfFrustumWidth = halfFrustumHeight * t_aspect;

    auto nearNorm = forward;
    pushConstCulling[1].plane[0] = LiteMath::to_float4(nearNorm, LiteMath::dot(nearNorm, forward * zNear + pos));

    auto farNorm = forward * -1.0f;
    pushConstCulling[1].plane[1] = LiteMath::to_float4(farNorm, LiteMath::dot(farNorm, forward * zFar + pos));

    auto topNorm = LiteMath::normalize(LiteMath::cross(right, forward * zFar - up * halfFrustumHeight));
    pushConstCulling[1].plane[2] = LiteMath::to_float4(topNorm, LiteMath::dot(topNorm, pos));

    auto bottomNorm = LiteMath::normalize(LiteMath::cross(forward * zFar + up * halfFrustumHeight, right));
    pushConstCulling[1].plane[3] = LiteMath::to_float4(bottomNorm, LiteMath::dot(bottomNorm, pos));

    auto rightNorm = LiteMath::normalize(LiteMath::cross(forward * zFar - right * halfFrustumWidth, up));
    pushConstCulling[1].plane[4] = LiteMath::to_float4(rightNorm, LiteMath::dot(rightNorm, pos));

    auto leftNorm = LiteMath::normalize(LiteMath::cross(up, forward * zFar + right * halfFrustumWidth));
    pushConstCulling[1].plane[5] = LiteMath::to_float4(leftNorm, LiteMath::dot(leftNorm, pos));
  }
}

void SimpleShadowmapRender::UpdateUniformBuffer(float a_time)
{
  m_uniforms.lightMatrix = m_lightMatrix;
  m_uniforms.lightPos    = m_light.cam.pos; //LiteMath::float3(sinf(a_time), 1.0f, cosf(a_time));
  m_uniforms.time        = a_time;

  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
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
