#include "../../utils/input_definitions.h"

#include "etna/Etna.hpp"
#include "deferred_render.hpp"

void DeferredRender::UpdateCamera(const Camera* cams, uint32_t a_camsNumber)
{
  m_cam = cams[0];
  UpdateView(); 
}

void DeferredRender::UpdateView()
{
  ///// calc camera matrix
  //
  const float aspect = float(m_width) / float(m_height);
  auto mProjFix = OpenglToVulkanProjectionMatrixFix();
  auto mProj = projectionMatrix(m_cam.fov, aspect, m_cam.nearPlane, m_cam.tdist);
  auto mLookAt = LiteMath::lookAt(m_cam.pos, m_cam.lookAt, m_cam.up);
  auto mWorldViewProj = mProjFix * mProj * mLookAt;

  pushConst2M1V.projMatInv = LiteMath::inverse4x4(mProjFix * mProj);
  pushConst2M1V.viewMatInv = LiteMath::inverse4x4(mLookAt);
  common->camPos = LiteMath::to_float4(m_cam.pos, 1.0f);
  
  m_worldViewProj = mWorldViewProj;

  {
    // camera
    auto pos = m_cam.pos;
    auto forward = m_cam.forward();
    auto up = LiteMath::normalize(m_cam.up);
    auto right = m_cam.right();
    
    float zNear = m_cam.nearPlane;
    float zFar = m_cam.tdist;
    float t_aspect = float(m_width) / float(m_height);
    
    float halfFrustumHeight = zFar * tanf(m_cam.fov * LiteMath::DEG_TO_RAD * 0.5f);
    float halfFrustumWidth = halfFrustumHeight * t_aspect;

    auto nearNorm = forward;
    pushConstCulling.plane[0] = LiteMath::to_float4(nearNorm, LiteMath::dot(nearNorm, forward * zNear + pos));

    auto farNorm = forward * -1.0f;
    pushConstCulling.plane[1] = LiteMath::to_float4(farNorm, LiteMath::dot(farNorm, forward * zFar + pos));

    auto topNorm = LiteMath::normalize(LiteMath::cross(right, forward * zFar - up * halfFrustumHeight));
    pushConstCulling.plane[2] = LiteMath::to_float4(topNorm, LiteMath::dot(topNorm, pos));

    auto bottomNorm = LiteMath::normalize(LiteMath::cross(forward * zFar + up * halfFrustumHeight, right));
    pushConstCulling.plane[3] = LiteMath::to_float4(bottomNorm, LiteMath::dot(bottomNorm, pos));

    auto rightNorm = LiteMath::normalize(LiteMath::cross(forward * zFar - right * halfFrustumWidth, up));
    pushConstCulling.plane[4] = LiteMath::to_float4(rightNorm, LiteMath::dot(rightNorm, pos));

    auto leftNorm = LiteMath::normalize(LiteMath::cross(up, forward * zFar + right * halfFrustumWidth));
    pushConstCulling.plane[5] = LiteMath::to_float4(leftNorm, LiteMath::dot(leftNorm, pos));
  }
}

// void DeferredRender::UpdateUniformBuffer(float a_time)
// {
//   m_uniforms.lightMatrix = m_lightMatrix;
//   m_uniforms.lightPos    = m_light.cam.pos; //LiteMath::float3(sinf(a_time), 1.0f, cosf(a_time));
//   m_uniforms.time        = a_time;

//   memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
// }

void DeferredRender::ProcessInput(const AppInput &input)
{
  // add keyboard controls here
  // camera movement is processed separately
  //
  if (input.keyReleased[GLFW_KEY_N] && lightCount < lightMax) {
    // add light source
    m_lights[lightCount++] = {float4(0,4,0,1), float4(1,1,1,1)};
  }

  if (input.keyReleased[GLFW_KEY_K] && lightCount) {
    curLight = (curLight + 1) % lightCount;
  }

  if (input.keyReleased[GLFW_KEY_J] && lightCount) {
    curLight = curLight ? curLight - 1 : lightCount - 1;
  }

  common->lightCount = lightCount;

  // recreate pipeline to reload shaders
  if (input.keyPressed[GLFW_KEY_B])
  {
#ifdef WIN32
    std::system("cd ../resources/shaders && python compile_deferred_shading_shaders.py");
#else
    std::system("cd ../resources/shaders && python3 compile_deferred_shading_shaders.py");
#endif

    etna::reload_shaders();

    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_swapchain.GetAttachment(i).image, m_swapchain.GetAttachment(i).view);
    }
  }
}