#pragma once

#include <array>

#include "Camera.h"
#include "Context.h"
#include "InputManager.h"
#include "Settings.h"
#include "ipass/TessellationPass.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

class ScenePass
{
public:
  ScenePass(Context& context, ipass::ComputeTessPass& computeTessPass, ipass::VertexTessPass& vertexTessPass);
  ~ScenePass();
  void Execute(wgpu::RenderPassEncoder& encoder);

  void OnResize(glm::uvec2 size);

  void InitializeShaderVariants();
  void LoadPatches(const ipass::PatchData& data);

  wgpu::TextureView GetDepthTextureView() { return this->depthTextureView; }

  wgpu::BlendState GetBlendState();

private:
  Context& context;
  ipass::ComputeTessPass& computeTessPass;
  ipass::VertexTessPass&  vertexTessPass;

  glm::u32 vertexCount = 0;
  wgpu::Buffer vertexBuffer;

  Camera camera;

  wgpu::Texture depthTexture;
  wgpu::TextureView depthTextureView;
  void CreateDepthTexture(glm::uvec2 size);

  struct ShaderVariant {
    wgpu::ShaderModule shader;
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::BindGroup bindGroup;

    // vertex buffer path
    wgpu::PipelineLayout layout;
    wgpu::RenderPipeline pipeline;
    wgpu::RenderPipeline wireframePipeline;

    // direct draw path, with surface bindings at ipass::SURFACE_BIND_GROUP
    wgpu::PipelineLayout directLayout;
    wgpu::RenderPipeline directPipeline;
  };
  std::array<ShaderVariant, 4> shaderVariants;
  ShadingMode activeMode = ShadingMode::BlinnPhong;

  wgpu::Buffer wireframeIndexBuffer;
  glm::u32 wireframeIndexCount = 0;

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
  wgpu::Buffer colorBuffer;
  wgpu::Buffer controlPointsBuffer;
  bool ownsControlPointsBuffer = false;

  wgpu::PipelineLayout CreatePipelineLayout(std::initializer_list<wgpu::BindGroupLayout> bindGroupLayouts);
  wgpu::RenderPipeline CreatePipeline(wgpu::ShaderModule& shader,
                                      wgpu::PipelineLayout& pipelineLayout,
                                      const char* vertexEntryPoint,
                                      wgpu::PrimitiveTopology topology,
                                      const wgpu::VertexBufferLayout* vertexLayout);
                                      
  void BuildVariant(ShaderVariant& variant, const std::string& name, const std::string& source);

  void RebuildParametricErrorBindGroup();
};
