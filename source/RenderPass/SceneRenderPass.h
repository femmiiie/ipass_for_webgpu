#ifndef SCENERENDERPASS_H_
#define SCENERENDERPASS_H_

#include "RenderPass.h"

class Camera;

class SceneRenderPass : public RenderPass
{
public:
  SceneRenderPass(Renderer& context);
  ~SceneRenderPass();
  void Execute(wgpu::RenderPassEncoder& encoder) override;

  void SetCamera(Camera* cam) { camera = cam; }

  void InitializeRenderPipeline();

  wgpu::BlendState GetBlendState();
  wgpu::VertexAttribute CreateAttribute(glm::u32 location, wgpu::VertexFormat format, uint64_t offset);

private:
  Camera* camera = nullptr;

  struct Light
  {
    glm::vec4 position;
    glm::vec4 color;
    glm::f32 power;
  } light;
  wgpu::Buffer lightBuffer;

  struct MVP
  {
    glm::mat4 M;
    glm::mat4 V;
    glm::mat4 P;
  } mvp;  
  wgpu::Buffer mvpBuffer;
};

#endif