#include "UIRenderPass.h"
#include "Renderer.h"
#include "Settings.h"
#include "UIStyle.h"

#include <cstdio>
#include <cstring>
#include <vector>

UIRenderPass::UIRenderPass(Context& context, const std::string& fontPath) : RenderPass(context)
{
  this->uiScale = glm::max(1.0f, (float)context.size.y / 1080.0f);

	nk_font_atlas_init_default(&this->atlas);
	nk_font_atlas_begin(&this->atlas);

  if (!fontPath.empty())
  {
    struct nk_font_config config = nk_font_config(BASE_FONT_SIZE * MAX_FONT_SCALE);

    static const nk_rune ranges[] = {
      0x0020, 0x007F,  // standard ascii chars
      0x2264, 0x2265,  // ≤ and ≥
      0
    };
    config.range = ranges;

    this->scaledFont = nk_font_atlas_add_from_file(&this->atlas, fontPath.c_str(), BASE_FONT_SIZE * MAX_FONT_SCALE, &config);
  }

  if (!this->scaledFont)
    this->scaledFont = nk_font_atlas_add_default(&this->atlas, BASE_FONT_SIZE * MAX_FONT_SCALE, nullptr);

	int img_w, img_h;
	const void* image = nk_font_atlas_bake(&this->atlas, &img_w, &img_h, NK_FONT_ATLAS_RGBA32);

  size_t bakedSize = (size_t)(img_w * img_h * 4);
  std::vector<uint8_t> imageCopy(bakedSize);
  memcpy(imageCopy.data(), image, bakedSize);

  struct nk_draw_null_texture nullTex;
	nk_font_atlas_end(&this->atlas, nk_handle_id(0), &nullTex);

  this->scaledFont->handle.height = BASE_FONT_SIZE * this->uiScale;
	nk_init_default(&this->uiContext, &this->scaledFont->handle);
  this->uiContext.style = ApplyUIStyle(this->uiContext.style, this->uiScale);

  nk_buffer_init_default(&this->cmds);
  nk_buffer_init_default(&this->verts);
  nk_buffer_init_default(&this->idx);

  static const struct nk_draw_vertex_layout_element vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(Vertex, pos)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(Vertex, tex)},
    {NK_VERTEX_COLOR,    NK_FORMAT_R32G32B32A32_FLOAT, offsetof(Vertex, color)},
    {NK_VERTEX_LAYOUT_END}
  };

  memset(&this->convertConfig, 0, sizeof(this->convertConfig));
  this->convertConfig.vertex_layout = vertex_layout;
  this->convertConfig.vertex_size = sizeof(Vertex);
  this->convertConfig.vertex_alignment = alignof(Vertex);
  this->convertConfig.global_alpha = 1.0f;
  this->convertConfig.shape_AA = NK_ANTI_ALIASING_ON;
  this->convertConfig.line_AA = NK_ANTI_ALIASING_ON;
  this->convertConfig.circle_segment_count = 22;
  this->convertConfig.curve_segment_count = 22;
  this->convertConfig.arc_segment_count = 22;
  this->convertConfig.tex_null = nullTex;

  this->vertexBuffer = this->CreateBuffer(
    MAX_VERTEX_BUFFER_SIZE,
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex)),
    false
  );

  this->indexBuffer = this->CreateBuffer(
    MAX_INDEX_BUFFER_SIZE,
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index)),
    false
  );

  this->InitSampler();

  wgpu::TextureDescriptor textureDesc;
  textureDesc.size = wgpu::Extent3D(img_w, img_h, 1);
  textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
  textureDesc.usage = wgpu::TextureUsage(static_cast<WGPUTextureUsage>(wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding));
  textureDesc.dimension = wgpu::TextureDimension::_2D;
  textureDesc.mipLevelCount = 1;
  textureDesc.sampleCount = 1;
  this->texture = this->context.device.createTexture(textureDesc);

  wgpu::TextureViewDescriptor viewDesc;
  viewDesc.aspect = wgpu::TextureAspect::All;
  viewDesc.baseArrayLayer = 0;
  viewDesc.arrayLayerCount = 1;
  viewDesc.baseMipLevel = 0;
  viewDesc.mipLevelCount = 1;
  viewDesc.dimension = wgpu::TextureViewDimension::_2D;
  viewDesc.format = textureDesc.format;
  wgpu::TextureView view = texture.createView(viewDesc);

  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture;
  destination.mipLevel = 0;
  destination.origin = {0, 0, 0};
  destination.aspect = wgpu::TextureAspect::All;

  wgpu::TexelCopyBufferLayout source;
  source.offset = 0;
  source.bytesPerRow = 4 * textureDesc.size.width;
  source.rowsPerImage = textureDesc.size.height;

  size_t imageSize = (size_t)(img_w * img_h * 4);
  this->context.queue.writeTexture(destination, imageCopy.data(), imageSize, source, textureDesc.size);

  wgpu::BufferDescriptor projDesc;
  projDesc.size = 64;
  projDesc.usage = wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform));
  projDesc.mappedAtCreation = false;
  this->projectionBuffer = this->context.device.createBuffer(projDesc);

  this->UpdateProjection(this->context.size);

  wgpu::SamplerBindingType samplerType = wgpu::SamplerBindingType::Filtering;
  std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts {
    this->CreateSamplerLayout(0, wgpu::ShaderStage::Fragment, samplerType),
    this->CreateTextureLayout(1, wgpu::ShaderStage::Fragment),
    this->CreateBufferLayout(2, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, 64),
  };
  this->bindGroupLayout = this->CreateBindGroupLayout(bindingLayouts);

  wgpu::BindGroupEntry projEntry = wgpu::Default;
  projEntry.binding = 2;
  projEntry.buffer = this->projectionBuffer;
  projEntry.offset = 0;
  projEntry.size = 64;

  std::vector<wgpu::BindGroupEntry> bindings {
    this->CreateBinding(0, this->sampler),
    this->CreateBinding(1, view),
    projEntry
  };
  this->bindGroup = this->CreateBindGroup(bindings);

  this->InitializeRenderPipeline();
}

UIRenderPass::~UIRenderPass()
{
  nk_buffer_free(&this->cmds);
  nk_buffer_free(&this->verts);
  nk_buffer_free(&this->idx);

  nk_font_atlas_cleanup(&this->atlas);
	nk_free(&this->uiContext);

  this->pipeline.release();
  this->layout.release();
  this->bindGroupLayout.release();
  this->bindGroup.release();

  this->vertexBuffer.release();
  this->indexBuffer.release();
  this->projectionBuffer.release();

  this->texture.destroy();
  this->texture.release();
}

void UIRenderPass::Execute(wgpu::RenderPassEncoder& encoder)
{
  this->RenderUI();

  nk_size vertexSize = this->verts.allocated;
  nk_size indexSize = this->idx.allocated;

  if (vertexSize > 0 && indexSize > 0)
  {
    nk_size alignedVtxSize = (vertexSize + 3) & ~(nk_size)3;
    nk_size alignedIdxSize = (indexSize + 3) & ~(nk_size)3;

    this->context.queue.writeBuffer(this->vertexBuffer, 0,
      nk_buffer_memory_const(&this->verts), alignedVtxSize);
    this->context.queue.writeBuffer(this->indexBuffer, 0,
      nk_buffer_memory_const(&this->idx), alignedIdxSize);

    encoder.setPipeline(this->pipeline);
    encoder.setVertexBuffer(0, this->vertexBuffer, 0, alignedVtxSize);
    encoder.setIndexBuffer(this->indexBuffer, wgpu::IndexFormat::Uint16, 0, alignedIdxSize);
    encoder.setBindGroup(0, this->bindGroup, 0, nullptr);

    uint32_t indexOffset = 0;
    const nk_draw_command* cmd;
    nk_draw_foreach(cmd, &this->uiContext, &this->cmds)
    {
      if (!cmd->elem_count) continue;
      encoder.drawIndexed(cmd->elem_count, 1, indexOffset, 0, 0);
      indexOffset += cmd->elem_count;
    }
  }

  nk_clear(&this->uiContext);
}

void UIRenderPass::InitializeRenderPipeline()
{
  wgpu::ShaderModule shaderModule = utils::LoadShader(this->context.device, "Pass/RenderPass/ui.wgsl");
  if (!shaderModule)
  {
    throw new RenderPassException("Failed to load shader module.");
  }

  wgpu::PipelineLayoutDescriptor layoutDesc;
  layoutDesc.bindGroupLayoutCount = 1;
  layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&(this->bindGroupLayout);
  this->layout = this->context.device.createPipelineLayout(layoutDesc);

  wgpu::RenderPipelineDescriptor pipelineDesc;
  pipelineDesc.layout = this->layout;

  std::vector<wgpu::VertexAttribute> vertexAttrs {
    this->CreateAttribute(0, wgpu::VertexFormat::Float32x2, 0),
    this->CreateAttribute(1, wgpu::VertexFormat::Float32x2, 2 * sizeof(glm::f32)),
    this->CreateAttribute(2, wgpu::VertexFormat::Float32x4, 4 * sizeof(glm::f32)),
  };

  wgpu::VertexBufferLayout vertexBufferLayout;
  vertexBufferLayout.attributeCount = vertexAttrs.size();
  vertexBufferLayout.attributes = vertexAttrs.data();
  vertexBufferLayout.arrayStride = 8 * sizeof(glm::f32);
  vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

  pipelineDesc.vertex.bufferCount = 1;
  pipelineDesc.vertex.buffers = &vertexBufferLayout;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  utils::SetEntryPoint(pipelineDesc.vertex.entryPoint, "vs_main");


  pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
  pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
  pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

  wgpu::BlendState blend = this->GetBlendState();

  wgpu::ColorTargetState colorTarget;
  colorTarget.format = this->context.colorFormat;
  colorTarget.blend = &blend;
  colorTarget.writeMask = wgpu::ColorWriteMask::All;

  wgpu::FragmentState fragmentState;
  fragmentState.module = shaderModule;
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;
  utils::SetEntryPoint(fragmentState.entryPoint, "fs_main");

  pipelineDesc.fragment = &fragmentState;

  wgpu::DepthStencilState uiDepthStencil = wgpu::Default;
  uiDepthStencil.format             = wgpu::TextureFormat::Depth24Plus;
  uiDepthStencil.depthWriteEnabled  = wgpu::OptionalBool::False;
  uiDepthStencil.depthCompare       = wgpu::CompareFunction::Always;
  uiDepthStencil.stencilReadMask    = 0;
  uiDepthStencil.stencilWriteMask   = 0;
  pipelineDesc.depthStencil = &uiDepthStencil;

  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  this->pipeline = this->context.device.createRenderPipeline(pipelineDesc);
  shaderModule.release();
}

wgpu::VertexAttribute UIRenderPass::CreateAttribute(glm::u32 location, wgpu::VertexFormat format, uint64_t offset)
{
  wgpu::VertexAttribute attr;
  attr.shaderLocation = location;
  attr.format = format;
  attr.offset = offset;
  return attr;
}

wgpu::BlendState UIRenderPass::GetBlendState()
{
  wgpu::BlendState state;
  state.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  state.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  state.color.operation = wgpu::BlendOperation::Add;
  state.alpha.srcFactor = wgpu::BlendFactor::Zero;
  state.alpha.dstFactor = wgpu::BlendFactor::One;
  state.alpha.operation = wgpu::BlendOperation::Add;
  return state;
}

void UIRenderPass::InitSampler()
{
  wgpu::SamplerDescriptor samplerDesc;
  samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
  samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
  samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
  samplerDesc.magFilter = wgpu::FilterMode::Linear;
  samplerDesc.minFilter = wgpu::FilterMode::Linear;
  samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
  samplerDesc.lodMinClamp = 0.0f;
  samplerDesc.lodMaxClamp = 1.0f;
  samplerDesc.compare = wgpu::CompareFunction::Undefined;
  samplerDesc.maxAnisotropy = 1;
  this->sampler = this->context.device.createSampler(samplerDesc);
}


void UIRenderPass::UpdateProjection(glm::uvec2 size)
{
  this->context.size = size;
  this->uiScale = glm::max(1.0f, (float)size.y / 1080.0f);

  if (this->scaledFont)
  {
    this->scaledFont->handle.height = BASE_FONT_SIZE * this->uiScale;
    nk_style_set_font(&this->uiContext, &this->scaledFont->handle);
  }

  glm::mat4 projection = glm::ortho(0.0f, (float)size.x, (float)size.y, 0.0f, -1.0f, 1.0f);
  this->context.queue.writeBuffer(this->projectionBuffer, 0, &projection, sizeof(projection));

  this->screenResized = true;
  this->lastScreenSize = size;
}

bool UIRenderPass::DrawCombo(std::vector<const char*> items, glm::u32& selected)
{
  bool changed = false;
  nk_context* ctx = &this->uiContext;
  float comboHeight = this->uiContext.style.font->height + this->uiContext.style.button.padding.y * 2.0f;
  float popupHeight = (float)items.size() * comboHeight + this->uiContext.style.window.padding.y * 2.0f;

  if (!ctx || items.empty())
    return false;

  if (selected >= items.size())
    selected = 0;


  if (nk_combo_begin_label(ctx, items[selected], nk_vec2(this->context.size.x / 4.0f - 16.0f * this->uiScale, popupHeight)))
  {
    nk_layout_row_dynamic(ctx, comboHeight, 1);
    for (glm::u32 i = 0; i < items.size(); ++i)
    {
      if (nk_combo_item_label(ctx, items[i], NK_TEXT_LEFT))
      {
        if (selected != i)
        {
          selected = i;
          changed = true;
        }
        nk_combo_close(ctx);
        break;
      }
    }
    nk_combo_end(ctx);
  }

  return changed;
}

void UIRenderPass::RenderUI()
{
  nk_color checkColor = nk_rgba_f(Settings::clearColor.r, Settings::clearColor.g, Settings::clearColor.b, 1.0f);
  this->uiContext.style.checkbox.cursor_normal = nk_style_item_color(checkColor);
  this->uiContext.style.checkbox.cursor_hover  = nk_style_item_color(checkColor);

  glm::vec2 menu_size(this->context.size);
  menu_size.x /= 4;

  RenderMainPanel(menu_size);
  RenderPerformanceWindow();
  RenderShadingLegend(menu_size);
  this->screenResized = false;

  nk_buffer_clear(&this->verts);
  nk_buffer_clear(&this->idx);
  nk_buffer_clear(&this->cmds);

  nk_flags convertResult = nk_convert(&this->uiContext, &this->cmds,
    &this->verts, &this->idx, &this->convertConfig);
  if (convertResult != NK_CONVERT_SUCCESS)
  {
    std::cerr << "nk_convert failed with code: " << convertResult << std::endl;
    nk_clear(&this->uiContext);
    return;
  }
}


void UIRenderPass::RenderMainPanel(glm::vec2 menu_size)
{
  nk_context* ctx = &this->uiContext;
  static nk_flags flags = NK_WINDOW_BORDER;

  if (nk_begin(ctx, "Test Window", nk_rect(0, 0, menu_size.x, menu_size.y), flags))
  {
    nk_layout_row_dynamic(ctx, 0, 2);
    
    nk_label(ctx, this->current_filename.c_str(), NK_TEXT_LEFT);
    if (nk_button_label(ctx, "Open File"))
      utils::OpenFile("BezierView File", "bv", [this](std::string s){
        size_t pos = s.find_last_of("/\\");
        this->current_filename = (pos != std::string::npos) ? s.substr(pos + 1) : s;
        Settings::parser.modify().Parse(s);
      });

    {
      nk_bool tess = Settings::tessellation.get() ? nk_true : nk_false;
      if (nk_checkbox_label(ctx, "Enable Tessellation", &tess))
        Settings::tessellation.modify() = (tess == nk_true);
    }

    nk_checkbox_label(ctx, "Show Performance Window", (nk_bool*)&Settings::perfWindow.get());

    if (nk_tree_push(ctx, NK_TREE_TAB, "Object Properties", NK_MINIMIZED))
    {
      RenderObjectPropertiesSection();
      nk_tree_pop(ctx);
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Settings", NK_MINIMIZED))
    {
      RenderSettingsSection(menu_size);
      nk_tree_pop(ctx);
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Debug Output", NK_MINIMIZED))
    {
      RenderDebugSection();
      nk_tree_pop(ctx);
    }

  }
  nk_end(&this->uiContext);
}

void UIRenderPass::ComponentLabel(nk_context* ctx, const char* text)
{
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, 1.0f));
  nk_layout_row_dynamic(ctx, 0, 1);
  nk_label(ctx, text, NK_TEXT_LEFT);
  nk_style_pop_vec2(ctx);
}

void UIRenderPass::RenderObjectPropertiesSection()
{
  nk_context* ctx = &this->uiContext;
  float s = this->uiScale;

  ComponentLabel(ctx, "Translation");
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, ctx->style.window.spacing.y * 0.5f));
  nk_property_float(ctx, "Trans X:", -1000.0f, &Settings::mvp.get().translation.x, 1000.0f, 0.01f, 0.01f);
  nk_property_float(ctx, "Trans Y:", -1000.0f, &Settings::mvp.get().translation.y, 1000.0f, 0.01f, 0.01f);
  nk_property_float(ctx, "Trans Z:", -1000.0f, &Settings::mvp.get().translation.z, 1000.0f, 0.01f, 0.01f);
  nk_style_pop_vec2(ctx);

  nk_layout_row_dynamic(ctx, 8.0f * s, 1);
  nk_spacer(ctx);

  ComponentLabel(ctx, "Rotation (degrees)");
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, ctx->style.window.spacing.y * 0.5f));
  nk_property_float(ctx, "Rot X:", -360.0f, &Settings::mvp.get().rotation.x, 360.0f, 1.0f, 0.5f);
  nk_property_float(ctx, "Rot Y:", -360.0f, &Settings::mvp.get().rotation.y, 360.0f, 1.0f, 0.5f);
  nk_property_float(ctx, "Rot Z:", -360.0f, &Settings::mvp.get().rotation.z, 360.0f, 1.0f, 0.5f);
  nk_style_pop_vec2(ctx);

  nk_layout_row_dynamic(ctx, 8.0f * s, 1);
  nk_spacer(ctx);

  ComponentLabel(ctx, "Scale");
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, ctx->style.window.spacing.y * 0.5f));
  nk_property_float(ctx, "Scale X:", 0.001f, &Settings::mvp.get().scale.x, 1000.0f, 0.05f, 0.01f);
  nk_property_float(ctx, "Scale Y:", 0.001f, &Settings::mvp.get().scale.y, 1000.0f, 0.05f, 0.01f);
  nk_property_float(ctx, "Scale Z:", 0.001f, &Settings::mvp.get().scale.z, 1000.0f, 0.05f, 0.01f);
  nk_style_pop_vec2(ctx);

  nk_layout_row_dynamic(ctx, 8.0f * s, 1);
  nk_spacer(ctx);

  nk_layout_row_static(ctx, 0, this->context.size.x / 10, 1);
  if (nk_button_label(ctx, "Reset Transform"))
  {
    Settings::mvp.get().translation = { 0.0f, 0.0f, 0.0f };
    Settings::mvp.get().rotation    = { 0.0f, 0.0f, 0.0f };
    Settings::mvp.get().scale       = { 1.0f, 1.0f, 1.0f };
    Settings::mvp.notify();
  }

  nk_layout_row_dynamic(ctx, 8.0f * s, 1);
  nk_spacer(ctx);
}

void UIRenderPass::RenderDebugSection()
{
  nk_context* ctx = &this->uiContext;

  float row_h = ctx->style.font->height;
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, 0.0f));

  if (this->debugData.empty())
  {
    nk_layout_row_dynamic(ctx, row_h, 1);
    nk_label(ctx, "No data available", NK_TEXT_LEFT);
  }
  else
  {
    char buf[64];
    constexpr size_t MAX_DISPLAY = 256;
    size_t displayCount = std::min(this->debugData.size(), MAX_DISPLAY);

    size_t entryCount = 0;
    for (size_t i = 0; i < displayCount; ++i)
    {
      if (this->debugData[i] == 0.0f) break;
      entryCount++;
    }

    int cols = std::min(4, std::max(1, (int)((entryCount + 7) / 8)));

    const auto& font = *ctx->style.font;
    float index_w = font.width(font.userdata, font.height, "[255]: ", 7);

    nk_layout_row_template_begin(ctx, row_h);
    for (int c = 0; c < cols; c++)
    {
      nk_layout_row_template_push_static(ctx, index_w);
      nk_layout_row_template_push_dynamic(ctx);
    }
    nk_layout_row_template_end(ctx);

    char valBuf[32];
    for (size_t i = 0; i < entryCount; ++i)
    {
      std::snprintf(buf, sizeof(buf), "[%3zu]:", i);
      std::snprintf(valBuf, sizeof(valBuf), "%10.4f", this->debugData[i]);
      nk_label(ctx, buf, NK_TEXT_LEFT);
      nk_label(ctx, valBuf, NK_TEXT_LEFT);
    }
  }
  nk_style_pop_vec2(ctx);
}

void UIRenderPass::RenderSettingsSection(glm::vec2 menu_size)
{
  nk_context* ctx = &this->uiContext;
  float s = this->uiScale;

  nk_layout_row_dynamic(ctx, 0, 1);
  nk_checkbox_label(ctx, "Wireframe", (nk_bool*)&Settings::wireframe.get());

    static std::vector<const char*> shaders = { "Blinn-Phong", "Flat", "Parametric Error", "Triangle Size" };
  glm::u32 curr = (glm::u32)Settings::shadingMode.get();
  float comboHeight = ctx->style.font->height + ctx->style.button.padding.y * 2.0f;
  ComponentLabel(ctx, "Shading Mode");
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, ctx->style.window.spacing.y * 0.5f));
  nk_layout_row_dynamic(ctx, comboHeight, 2);
  if (this->DrawCombo(shaders, curr))
    Settings::shadingMode.modify() = (ShadingMode)curr;
  nk_spacer(ctx);
  nk_style_pop_vec2(ctx);

  ComponentLabel(ctx, "Present Mode");

  #ifdef __EMSCRIPTEN__
  nk_label(ctx, "Uncapping framerate not supported on Web.", NK_TEXT_LEFT);
  #else
  static std::vector<const char*> presentModes = { "Capped", "Uncapped", "Low-latency" };
  curr = (int)Settings::presentMode.get();
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(ctx->style.window.spacing.x, ctx->style.window.spacing.y * 0.5f));
  nk_layout_row_dynamic(ctx, comboHeight, 2);
  if (this->DrawCombo(presentModes, curr))
    Settings::presentMode.modify() = (PresentModeSetting)curr;
  nk_spacer(ctx);
  nk_style_pop_vec2(ctx);
  #endif

  nk_layout_row_dynamic(ctx, 8.0f * s, 1);
  nk_spacer(ctx);

  ComponentLabel(ctx, "Background Color");

  nk_colorf color = {
    Settings::clearColor.r,
    Settings::clearColor.g,
    Settings::clearColor.b,
    Settings::clearColor.a
  };

  float pickerSize = glm::min(menu_size.x - 16.0f * s, 200.0f * s);
  nk_layout_row_dynamic(ctx, pickerSize, 1);
  color = nk_color_picker(ctx, color, NK_RGBA);
  Settings::clearColor = { color.r, color.g, color.b, color.a };
}

void UIRenderPass::RenderShadingLegend(glm::vec2 menu_size)
{
  ShadingMode activeMode = Settings::shadingMode.get();
  if (activeMode != ShadingMode::ParametricError && activeMode != ShadingMode::TriangleSize)
    return;

  nk_context* ctx = &this->uiContext;
  float s = this->uiScale;

  const char* title = "Shader Key";

  const auto& font = *ctx->style.font;
  const auto& win  = ctx->style.window;

  float swatch_w = 16.0f * s;
  float text_w   = font.width(font.userdata, font.height, ">= 20 px", 8) + 8.0f * s;
  
  float width = swatch_w + text_w + win.padding.x * 2.0f;

  float row_height    = std::max(font.height, swatch_w) * 1.1f;
  float header_height = font.height + win.header.padding.y * 2.0f + win.header.label_padding.y * 2.0f;
  float height   = header_height + row_height * 4.0f + win.padding.y * 4.0f;

  float margin = 8.0f * s;
  float x  = menu_size.x + margin;

  struct nk_rect window_rect = nk_rect(x, margin, width, height);

  if (this->screenResized)
  {
    nk_window_set_bounds(ctx, title, window_rect);
  }

  if (nk_begin(ctx, title, window_rect, this->subwindowFlags))
  {
    struct Entry { nk_color color; const char* label; };
    Entry entries[4];

    if (activeMode == ShadingMode::ParametricError)
    {
      entries[0] = { nk_rgb(255, 255, 255), "≤ 0.1 px" };
      entries[1] = { nk_rgb(  0,   0, 255), "≤ 0.5 px" };
      entries[2] = { nk_rgb(  0, 255,   0), "≤ 1.0 px" };
      entries[3] = { nk_rgb(255,   0,   0), "> 1.0 px" };
    }
    else
    {
      entries[0] = { nk_rgb(255, 255, 255), "≥ 20 px" };
      entries[1] = { nk_rgb(255,   0,   0), "< 20 px" };
      entries[2] = { nk_rgb(  0, 255,   0), "< 10 px" };
      entries[3] = { nk_rgb(  0,   0, 255), "< 5 px"  };
    }

    for (int i = 0; i < 4; i++)
    {
      nk_layout_row_begin(ctx, NK_STATIC, row_height, 2);
      nk_layout_row_push(ctx, swatch_w);
      nk_button_color(ctx, entries[i].color);
      nk_layout_row_push(ctx, text_w);
      nk_label(ctx, entries[i].label, NK_TEXT_LEFT);
      nk_layout_row_end(ctx);
    }
  }
  nk_end(ctx);
}

void UIRenderPass::RenderPerformanceWindow()
{
  if (!Settings::perfWindow.get())
    return;

  nk_context* ctx = &this->uiContext;
  float s = this->uiScale;

  char fpsBuf[64];
  std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", this->context.performance.fps);

  char ftBuf[64];
  std::snprintf(ftBuf, sizeof(ftBuf), "Frame: %.2f ms", this->context.performance.avg_frametime);

  bool tessActive = Settings::tessellation.get();
  const auto& tessOut = Settings::tessOutput.get();

  char triBuf[64]   = "Triangles: 0 (Fix me!)";
  char patchBuf[64] = {};
  if (tessActive)
  {
    // uint32_t triCount = tessOut.vertexCount / 3;
    // std::snprintf(triBuf,   sizeof(triBuf),   "Triangles: %u", triCount);
    std::snprintf(patchBuf, sizeof(patchBuf), "Patches: %u",   tessOut.patchCount);
  }

  const auto& font = *ctx->style.font;
  const auto& win  = ctx->style.window;

  const char* widthTemplate = "Triangles: 00000000";
  float perf_width = font.width(font.userdata, font.height, widthTemplate, (int)strlen(widthTemplate)) * 1.4f;

  float row_height    = font.height * 1.1f;
  float header_height = font.height + win.header.padding.y * 2.0f + win.header.label_padding.y * 2.0f;
  int row_count = 2 + (tessActive ? 2 : 0);
  float content_height = row_height * row_count + win.spacing.y * glm::max(0, row_count - 1);
  float perf_height = header_height + content_height + win.padding.y * 2.0f;

  float margin = 8.0f * s;
  float perf_x  = (float)this->context.size.x - perf_width - margin;

  struct nk_rect window_rect = nk_rect(perf_x, margin, perf_width, perf_height);

  static int last_row_count = -1;
  if (this->screenResized || last_row_count != row_count)
  {
    nk_window_set_bounds(ctx, "Performance", window_rect);
    last_row_count = row_count;
  }

  if (nk_begin(ctx, "Performance", window_rect, this->subwindowFlags))
  {
    nk_layout_row_static(ctx, row_height, (int)perf_width, 1);
    nk_label(ctx, fpsBuf, NK_TEXT_LEFT);
    nk_label(ctx, ftBuf, NK_TEXT_LEFT);
    if (tessActive)
    {
      nk_label(ctx, triBuf,   NK_TEXT_LEFT);
      nk_label(ctx, patchBuf, NK_TEXT_LEFT);
    }
  }
  nk_end(ctx);
}