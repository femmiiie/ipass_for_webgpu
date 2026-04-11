#pragma once

#include <array>

#include "Camera.h"
#include "InputManager.h"
#include "RenderPass.h"
#include "Settings.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

class SceneRenderPass : public RenderPass
{
public:
  SceneRenderPass(Context& context);
  ~SceneRenderPass();
  void Execute(wgpu::RenderPassEncoder& encoder) override;

  void OnResize(glm::uvec2 size);

  void InitializeShaderVariants();
  void LoadBV(const BVParser& parser);

  void UseGPUTessellated(wgpu::Buffer buf, uint32_t count);

  wgpu::TextureView GetDepthTextureView() { return this->depthTextureView; }

  wgpu::BlendState GetBlendState();
  wgpu::VertexAttribute CreateAttribute(glm::u32 location, wgpu::VertexFormat format, uint64_t offset);

private:
  Camera camera;

  wgpu::Texture depthTexture;
  wgpu::TextureView depthTextureView;
  void CreateDepthTexture(glm::uvec2 size);

  struct ShaderVariant {
    wgpu::RenderPipeline pipeline;
    wgpu::RenderPipeline wireframePipeline;
    wgpu::PipelineLayout layout;
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::BindGroup bindGroup;
  };
  std::array<ShaderVariant, 4> shaderVariants;
  ShadingMode activeMode = ShadingMode::BlinnPhong;

  wgpu::Buffer wireframeIndexBuffer;
  glm::u32 wireframeIndexCount = 0;

  bool ownsVertexBuffer = false;

  struct LightEntry
  {
    glm::vec4 position;
    glm::vec4 color;
    glm::f32  power;
    glm::f32  _pad[3] = {0.0, 0.0, 0.0};
  };
  struct LightsData
  {
    LightEntry lights[4];
  } lightsData;

  struct ViewportData {
    glm::f32 x, y, width, height;
  } viewportData;

  wgpu::Buffer mvpBuffer;
  wgpu::Buffer lightBuffer;
  wgpu::Buffer viewportBuffer;
  wgpu::Buffer controlPointsBuffer;
  bool ownsControlPointsBuffer = false;

  wgpu::RenderPipeline CreatePipeline(wgpu::ShaderModule& shader,
                                      wgpu::PipelineLayout& pipelineLayout,
                                      wgpu::PrimitiveTopology topology);

  void RebuildParametricErrorBindGroup();
};
