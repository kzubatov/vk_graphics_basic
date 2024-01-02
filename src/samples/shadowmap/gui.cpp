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
    ImGui::SliderFloat3("Light source position", m_light.cam.pos.M, -10.f, 10.f);
    ImGui::SliderFloat3("Quad xz scale and y offset", pushConstQuad.scaleAndOffset.M, 0.f, 32.f);
    ImGui::SliderInt("Tessellation level", &pushConstQuad.tes_level, 1, 1024);

    if (ImGui::SliderFloat("Max height", &pushConstQuad.maxHeight, pushConstQuad.minHeight + 0.05f, 5.f))
      m_heightPass.recreate = true;

    if (ImGui::SliderFloat("Min height", &pushConstQuad.minHeight, -5.f, pushConstQuad.maxHeight - 0.05f))
      m_heightPass.recreate = true;

    if (ImGui::SliderFloat("Noise scale", &m_heightPass.pushConst.scale, 1.0f, 64.0f))
      m_heightPass.recreate = true;
    
    if (ImGui::Button("Update noise"))
    {
      m_heightPass.recreate = true;
      m_heightPass.pushConst.seed = float(std::rand()) / RAND_MAX;
    }    

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
