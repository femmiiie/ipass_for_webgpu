#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <nuklear.h>

#include "RenderPass.h"
#include "File.h"

#include <string>

using Vertex = utils::Vertex2D;

class UIRenderPass : public RenderPass
{
public:

  static constexpr uint32_t MAX_VERTEX_COUNT = 65536;
  static constexpr uint32_t MAX_INDEX_COUNT = 131072;
  static constexpr uint64_t MAX_VERTEX_BUFFER_SIZE = MAX_VERTEX_COUNT * sizeof(Vertex);
  static constexpr uint64_t MAX_INDEX_BUFFER_SIZE = MAX_INDEX_COUNT * sizeof(glm::u16);

  static constexpr float BASE_FONT_SIZE  = 13.0f;
  static constexpr float MAX_FONT_SCALE  = 4.0f;

  UIRenderPass(Context& context, const std::string& fontPath = "");
  ~UIRenderPass();
  void Execute(wgpu::RenderPassEncoder& encoder) override;
  void RenderUI();

  void InitSampler();
  void InitializeRenderPipeline();
  void UpdateProjection(glm::uvec2 size);

  wgpu::BlendState GetBlendState();
  wgpu::VertexAttribute CreateAttribute(glm::u32 location, wgpu::VertexFormat format, uint64_t offset);

  nk_context* getUIContext() { return &uiContext; }
  void SetDebugData(const std::vector<float>& data);

private:
  void ApplyScaledStyles();
  void RenderMainPanel(glm::vec2 menu_size);
  void RenderObjectPropertiesSection();
  void RenderDebugSection();
  void RenderSettingsSection(glm::vec2 menu_size);
  void RenderShadingLegend(glm::vec2 menu_size);
  void RenderPerformanceWindow();
  bool DrawCombo(std::vector<const char*> items, int& selected);

  nk_context uiContext;
  nk_font_atlas atlas;
  nk_buffer cmds, verts, idx;
  nk_convert_config convertConfig;

  float uiScale = 1.0f;
  nk_font* scaledFont = nullptr;

  std::string current_filename = "No Object Loaded";
  std::vector<float> debugData;

  glm::uvec2 lastScreenSize = glm::uvec2(0, 0);
  bool screenResized = false;

  wgpu::Sampler sampler;
  wgpu::Texture texture;
  wgpu::Buffer indexBuffer;
  wgpu::Buffer projectionBuffer;

  nk_flags subwindowFlags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE;
};
