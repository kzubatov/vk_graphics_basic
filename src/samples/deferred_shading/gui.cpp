#include "deferred_render.hpp"

#include "../../render/render_gui.h"

void DeferredRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  
  // std::cout << "here" << std::endl;
  {
  //  ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::Text("Lights in scene: %u", lightCount);
    ImGui::NewLine();

    // if (lightCount) {
    //   ImGui::ColorEdit3("Light color", (float *) &m_lights[curLight].colorAndRadius, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    //   ImGui::SliderFloat3("Light source position", (float *) &m_lights[curLight].position, -100.f, 100.f);
    //   ImGui::SliderFloat("Light source radius", (float *) &m_lights[curLight].colorAndRadius[3], 0.0f, 100.f);
    // }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }
  // std::cout << "here" << std::endl;

  // Rendering
  ImGui::Render();
}