#ifndef UIRENDERPASS_H_
#define UIRENDERPASS_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <nuklear.h>

#include "RenderPass.h"
#include "Utils.h"



class UIRenderPass : public RenderPass
{
public:
  struct Vertex {
    glm::vec2 pos;
    glm::vec2 tex;
    glm::vec4 col;
  };

  static constexpr uint32_t MAX_VERTEX_COUNT = 65536;
  static constexpr uint32_t MAX_INDEX_COUNT = 131072;
  static constexpr uint64_t MAX_VERTEX_BUFFER_SIZE = MAX_VERTEX_COUNT * sizeof(Vertex);
  static constexpr uint64_t MAX_INDEX_BUFFER_SIZE = MAX_INDEX_COUNT * sizeof(glm::u16);

  UIRenderPass(Renderer& context);
  ~UIRenderPass();
  void Execute(wgpu::RenderPassEncoder& encoder) override;
  void RenderUI();

  void InitSampler();
  void InitializeRenderPipeline();
  void UpdateProjection(glm::uvec2 size);

  wgpu::BlendState GetBlendState();
  wgpu::VertexAttribute CreateAttribute(glm::u32 location, wgpu::VertexFormat format, uint64_t offset);

  nk_context* getUIContext() { return &uiContext; }

private:
  nk_context uiContext;
  nk_font_atlas atlas;
  nk_buffer cmds, verts, idx;
  nk_convert_config convertConfig;

  wgpu::Sampler sampler;
  wgpu::Texture texture;
  wgpu::Buffer indexBuffer;
  wgpu::Buffer projectionBuffer;
};

#endif