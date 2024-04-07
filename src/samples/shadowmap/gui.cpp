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
    ImGui::SliderFloat3("Quad xz scale and y offset", pushConstQuad.scaleAndOffset.M, -10.f, 10.f);
    ImGui::SliderInt("Tessellation level", &pushConstQuad.tes_level, 1, 1024);

    if (ImGui::SliderFloat("Max height", &pushConstQuad.maxHeight, pushConstQuad.minHeight + 0.05f, 5.f))
      m_heightPass.recreate = true;

    if (ImGui::SliderFloat("Min height", &pushConstQuad.minHeight, -5.f, pushConstQuad.maxHeight - 0.05f))
      m_heightPass.recreate = true;

    if (ImGui::SliderFloat("Height frequency", &m_heightPass.pushConst.red_noise_scale, 1.0f, 16.0f))
      m_heightPass.recreate = true;
    
    if (ImGui::Button("Recreate noise"))
    {
      m_heightPass.recreate = true;
      m_heightPass.pushConst.red_y_scale = 10000 + std::rand() % 12000;
    }

    int polyId = m_polyMode == vk::PolygonMode::eLine;
    std::array<const char *, 2> types = {"Fill", "Line"};
    if (ImGui::ListBox("Polygon mode", &polyId, types.data(), 2))
    {
      m_polyMode = polyId ? vk::PolygonMode::eLine : vk::PolygonMode::eFill;
      SetupSimplePipeline();
    }

    if (polyId && ImGui::SliderInt("Line Width", &m_lineWidth, 1, 4))
    {
      SetupSimplePipeline();
    }

    ImGui::SliderInt("field length", &field_length, 1, 12);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
