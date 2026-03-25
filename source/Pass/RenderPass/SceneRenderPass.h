#pragma once

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

  void InitializeRenderPipeline();
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

  wgpu::RenderPipeline wireframePipeline;

  wgpu::Buffer wireframeIndexBuffer;
  glm::u32 wireframeIndexCount = 0;

  bool ownsVertexBuffer = false;

  struct Light
  {
    glm::vec4 position;
    glm::vec4 color;
    glm::f32 power;
  } light;
  wgpu::Buffer lightBuffer;
  wgpu::Buffer mvpBuffer;
};
