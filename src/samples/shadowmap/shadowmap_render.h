#ifndef SIMPLE_SHADOWMAP_RENDER_H
#define SIMPLE_SHADOWMAP_RENDER_H

#include "../../render/scene_mgr.h"
#include "../../render/render_common.h"
#include "../../../resources/shaders/common.h"
#include "etna/GraphicsPipeline.hpp"
#include <geom/vk_mesh.h>
#include <vk_descriptor_sets.h>
#include <vk_fbuf_attachment.h>
#include <vk_images.h>
#include <vk_swapchain.h>
#include <vk_quad.h>

#include <string>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Sampler.hpp>


class IRenderGUI;

class SimpleShadowmapRender : public IRender
{
public:
  SimpleShadowmapRender(uint32_t a_width, uint32_t a_height);
  ~SimpleShadowmapRender();

  uint32_t     GetWidth()      const override { return m_width; }
  uint32_t     GetHeight()     const override { return m_height; }
  VkInstance   GetVkInstance() const override { return m_context->getInstance(); }

  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR &a_surface, bool initGUI) override;

  void ProcessInput(const AppInput& input) override;
  void UpdateCamera(const Camera* cams, uint32_t a_camsNumber) override;
  Camera GetCurrentCamera() override {return m_cam;}
  void UpdateView();

  void LoadScene(const char *path, bool transpose_inst_matrices) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;

private:
  etna::GlobalContext* m_context;
  etna::Image mainViewDepth;
  etna::Image shadowMap;
  etna::Sampler defaultSampler;
  etna::Sampler linearSampler;
  etna::Buffer constants;
  etna::Image AAimage;
  etna::Image TAAHistoryBuffer;
  etna::Image velocityBuffer;
  bool m_useStencil;

  VkCommandPool    m_commandPool    = VK_NULL_HANDLE;

  struct
  {
    uint32_t    currentFrame      = 0u;
    VkQueue     queue             = VK_NULL_HANDLE;
    VkSemaphore imageAvailable    = VK_NULL_HANDLE;
    VkSemaphore renderingFinished = VK_NULL_HANDLE;
  } m_presentationResources;

  std::vector<VkFence> m_frameFences;
  std::vector<VkCommandBuffer> m_cmdBuffersDrawMain;

  struct
  {
    float4x4 projView;
    float4x4 model;
  } pushConst2M;

  enum AA
  {
    NoAA,
    SSAA,
    MSAA,
    TAA
  };

  AA m_AAType = AA::TAA;
  
  float2 HaltonSequence[8] = {{0.5f, 1.f / 3.f}, {0.25f, 2.f / 3.f},
                              {0.75f, 1.f / 9.f}, {0.125f, 4.f / 9.f},
                              {0.625f, 7.f / 9.f}, {0.375f, 2.f / 9.f},
                              {0.875, 5.f / 9.f}, {0.0625f, 8.f / 9.f}};

  uint32_t HaltonCounter = 0;
  etna::Buffer taa_info_buffer;
  
  struct
  {
    float4x4 prevProjViewWorld;
    float2 jitter;
  } taa_info;

  uint32_t sphere_index = 5u;

  void *m_taaInfoMappedMem = nullptr;

  float4x4 m_prevWorldViewProj;
  // float4x4 m_prevModelMatrix;

  float4x4 m_worldViewProj;
  float4x4 m_lightMatrix;

  UniformParams m_uniforms {};
  void* m_uboMappedMem = nullptr;

  bool m_recreateForwardPipelineAndImages = false;
  bool m_clearHistoryBuffer = false;

  etna::GraphicsPipeline m_basicForwardPipeline {};
  etna::GraphicsPipeline m_dynamicForwardPipeline {};
  etna::GraphicsPipeline m_resolvePipeline {};
  etna::GraphicsPipeline m_shadowPipeline {};

  std::shared_ptr<vk_utils::DescriptorMaker> m_pBindings = nullptr;
  
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VulkanSwapChain m_swapchain;

  Camera   m_cam;
  uint32_t m_width  = 1024u;
  uint32_t m_height = 1024u;
  uint32_t m_framesInFlight = 2u;
  bool m_vsync = false;

  vk::PhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char*> m_deviceExtensions;
  std::vector<const char*> m_instanceExtensions;

  std::shared_ptr<SceneManager>     m_pScnMgr;
  std::shared_ptr<IRenderGUI> m_pGUIRender;
  
  std::shared_ptr<vk_utils::IQuad>               m_pFSQuad;
  VkDescriptorSet       m_quadDS; 
  VkDescriptorSetLayout m_quadDSLayout = nullptr;

  struct InputControlMouseEtc
  {
    bool drawFSQuad = false;
  } m_input;

  /**
  \brief basic parameters that you usually need for shadow mapping
  */
  struct ShadowMapCam
  {
    ShadowMapCam() 
    {  
      cam.pos    = float3(4.0f, 4.0f, 4.0f);
      cam.lookAt = float3(0, 0, 0);
      cam.up     = float3(0, 1, 0);
  
      radius          = 5.0f;
      lightTargetDist = 20.0f;
      usePerspectiveM = true;
    }

    float  radius;           ///!< ignored when usePerspectiveM == true 
    float  lightTargetDist;  ///!< identify depth range
    Camera cam;              ///!< user control for light to later get light worldViewProj matrix
    bool   usePerspectiveM;  ///!< use perspective matrix if true and ortographics otherwise
  
  } m_light;
 
  void DrawFrameSimple(bool draw_gui);

  void CreateInstance();
  void CreateDevice(uint32_t a_deviceId);

  void BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView);

  void DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, 
    VkPipelineLayout a_pipelineLayout = VK_NULL_HANDLE, bool a_ignoreDynamic = false);

  void loadShaders();

  void SetupSimplePipeline();
  void RecreateSwapChain();
  void RecreateResolvePassResources();

  void UpdateUniformBuffer(float a_time);
  void UpdateTAAInfo();

  void SetupDeviceExtensions();

  void AllocateResources();
  void PreparePipelines();

  void DestroyPipelines();
  void DeallocateResources();

  void InitPresentStuff();
  void ResetPresentStuff();
  void SetupGUIElements();
};


#endif //CHIMERA_SIMPLE_RENDER_H
