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

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");

    auto prevAAType = m_AAType;
    if (ImGui::Button("No AA"))
      m_AAType = NoAA;

    if (ImGui::Button("SSAA 4x"))
      m_AAType = SSAA;

    if (ImGui::Button("MSAA 4x"))
      m_AAType = MSAA;
    
    if (ImGui::Button("TAA"))
      m_AAType = TAA;

    m_recreateForwardPipelineAndImages = prevAAType != m_AAType;
    m_clearHistoryBuffer = m_recreateForwardPipelineAndImages && m_AAType == TAA;

    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
