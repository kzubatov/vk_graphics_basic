#pragma once
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
#include <etna/TextureLoader.hpp>

class IRenderGUI;

class DeferredRender : public IRender
{
public:
  DeferredRender(uint32_t a_width, uint32_t a_height);
  ~DeferredRender();

  uint32_t     GetWidth()      const override { return m_width; }
  uint32_t     GetHeight()     const override { return m_height; }
  VkInstance   GetVkInstance() const override { return m_context->getInstance(); }

  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR &a_surface, bool initGUI) override;

  void ProcessInput(const AppInput& input) override;
  void UpdateCamera(const Camera* cams, uint32_t/*  a_camsNumber */) override;
  Camera GetCurrentCamera() override {return m_cam;}
  void UpdateView();

  void LoadScene(const char *path, bool transpose_inst_matrices) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;
private:
  etna::GlobalContext* m_context;
  
  etna::Buffer instanceMatrices;
  void* m_instanceMatricesMap;

  struct mInfo {
    float4 center;
    float radius;
    uint32_t count = 0;
    uint32_t bytes[2];
  };
  uint32_t m_groupCountX = 0;

  etna::Buffer meshInfo;
  void* m_meshInfoMap;
  
  etna::Buffer drawIndirectInfo;

  etna::Buffer visibleInstances;
  etna::Image mainViewDepth;
  etna::Image colorMap;
  etna::Image normalMap;
  etna::Sampler defaultSampler;
 
  etna::Texture bunnyAlbedoTexture;
  etna::Texture bunnyNormalTexture;
  etna::Texture bunnyRoughnessTexture;

  etna::Texture teapotAlbedoTexture;
  etna::Texture teapotNormalTexture;
  etna::Texture teapotMetalnessTexture;
  etna::Texture teapotRoughnessTexture;

  etna::Texture flatAlbedoTexture;
  etna::Texture flatNormalTexture;
  etna::Texture flatRoughnessTexture;
  
  
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
    uint32_t id;
    uint32_t bytes[3];
    float3 cameraPos;
    uint32_t bytes1;
  } pushConst;

  struct CommonParams
  {
    float4 camPos;
    uint32_t lightCount;
    uint32_t width;
    uint32_t bytes[2];
  };

  etna::Buffer uniformBuffer;
  CommonParams *common;

  float4x4 m_worldViewProj;

  etna::GraphicsPipeline m_gBufferPipeline {};
  etna::GraphicsPipeline m_finalPassPipeline {};

  etna::ComputePipeline m_cullingComputePipeline {};
  etna::ComputePipeline m_lightCullingComputePipeline {};
  
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

  std::shared_ptr<SceneManager> m_pScnMgr;
  std::shared_ptr<IRenderGUI> m_pGUIRender;
  std::shared_ptr<etna::TextureLoader> m_pTextureLoader;
  
  struct cullingParams {
    float4 plane[6];
  } pushConstCulling;

  struct {
    float4x4 projMatInv;
    float4x4 viewMatInv;
  } pushConst2M1V;

  struct lightParams {
    float4 position;
    float4 colorAndRadius;
  };

  etna::Buffer lightBuffer;
  etna::Buffer visibleLights;
  lightParams *m_lights;
  uint32_t lightCount = 0;
  uint32_t curLight = 0;
  uint32_t lightMax = 1 << 16;
 
  void DrawFrameSimple(bool draw_gui);

  void BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView);

  void DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp);

  void CullSceneCmd(VkCommandBuffer a_cmdBuff);

  void DrawFrameCmd(VkCommandBuffer a_cmdBuff);

  void loadShaders();

  void RecreateSwapChain();

  void SetupDeviceExtensions();

  void AllocateResources();

  void SetupPipelines();

  void DeallocateResources();

  void InitPresentStuff();

  void ResetPresentStuff();

  void SetupGUIElements();

  void PrepareCullingResources();

  void LoadTextures();
};