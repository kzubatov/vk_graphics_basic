#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source position", m_uniforms.lightPos.M, -10.f, 10.f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    if (ImGui::SliderFloat("Noise scale", &m_heightMapInfo.pushConst.noise_scale, 1.0f, 10.0f, "%.0f"))
      m_heightMapInfo.recreate = true;
    if (ImGui::SliderInt("Noise seed", &m_heightMapInfo.pushConst.y_scale, 10000, 30000))
      m_heightMapInfo.recreate = true;

    ImGui::SliderInt("Quad's half length", reinterpret_cast<int *>(&m_tessParams.quadHalfLength), 1, 32);
    ImGui::SliderInt("Quad's Height", reinterpret_cast<int *>(&m_tessParams.quadHeight), 1, 16);
    ImGui::SliderInt("Sqrt of path count", reinterpret_cast<int *>(&m_tessParams.sqrtPatchCount), 1, 16);
    ImGui::SliderInt("Tessellation, triangle size", reinterpret_cast<int *>(&m_tessParams.triangleSize), 5, 40);
    ImGui::SliderInt("Tessellation, min level", reinterpret_cast<int *>(&m_tessParams.tessMinLevel), 1, m_tessParams.tessMaxLevel);
    ImGui::SliderInt("Tessellation, max level", reinterpret_cast<int *>(&m_tessParams.tessMaxLevel), 
      m_tessParams.tessMinLevel, deviceProperties.limits.maxTessellationGenerationLevel);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
