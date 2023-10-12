#include "shadowmap_render.h"

#include "../../utils/input_definitions.h"

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

    SetupSimplePipeline();

    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_frameBuffers[i],
                               m_swapchain.GetAttachment(i).view, m_basicForwardPipeline.pipeline);
    }
  }
}

void SimpleShadowmapRender::UpdateCamera(const Camera* cams, uint32_t a_camsNumber)
{
  m_cam = cams[0];
  if(a_camsNumber >= 2)
    m_light.cam = cams[1];
  UpdateView(); 
  UpdateFrustum(cams[0], 0);
  UpdateFrustum(cams[1], 1);
}

void SimpleShadowmapRender::UpdateFrustum(const Camera &cam, uint32_t a_camID) {
  auto pos = cam.pos;
  auto forward = cam.forward();
  auto up = LiteMath::normalize(cam.up);
  auto right = cam.right();
  
  float zNear = a_camID ? 1.0f : cam.nearPlane;
  float zFar = a_camID ? m_light.lightTargetDist * 2.0f : cam.tdist;
  float t_aspect = a_camID ? 1.0f : float(m_width) / float(m_height);
  
  float halfFrustumHeight = zFar * tanf(cam.fov * LiteMath::DEG_TO_RAD * 0.5f);
  float halfFrustumWidth = halfFrustumHeight * t_aspect;

  auto nearNorm = forward;
  m_frustum[a_camID].plane[0] = LiteMath::to_float4(nearNorm, LiteMath::dot(nearNorm, forward * zNear + pos));

  auto farNorm = forward * -1.0f;
  m_frustum[a_camID].plane[1] = LiteMath::to_float4(farNorm, LiteMath::dot(farNorm, forward * zFar + pos));

  auto topNorm = LiteMath::normalize(LiteMath::cross(right, forward * zFar - up * halfFrustumHeight));
  m_frustum[a_camID].plane[2] = LiteMath::to_float4(topNorm, LiteMath::dot(topNorm, pos));

  auto bottomNorm = LiteMath::normalize(LiteMath::cross(forward * zFar + up * halfFrustumHeight, right));
  m_frustum[a_camID].plane[3] = LiteMath::to_float4(bottomNorm, LiteMath::dot(bottomNorm, pos));

  auto rightNorm = LiteMath::normalize(LiteMath::cross(forward * zFar - right * halfFrustumWidth, up));
  m_frustum[a_camID].plane[4] = LiteMath::to_float4(rightNorm, LiteMath::dot(rightNorm, pos));

  auto leftNorm = LiteMath::normalize(LiteMath::cross(up, forward * zFar + right * halfFrustumWidth));
  m_frustum[a_camID].plane[5] = LiteMath::to_float4(leftNorm, LiteMath::dot(leftNorm, pos));
};

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