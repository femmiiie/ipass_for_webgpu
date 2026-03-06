#include "UIRenderPass.h"
#include "Renderer.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <vector>

UIRenderPass::UIRenderPass(Context& context) : RenderPass(context)
{
  this->uiScale = glm::max(1.0f, (float)context.size.y / 1080.0f);

	nk_font_atlas_init_default(&this->atlas);
	nk_font_atlas_begin(&this->atlas);

	this->scaledFont = nk_font_atlas_add_default(&atlas, BASE_FONT_SIZE * MAX_FONT_SCALE, nullptr);

	int img_w, img_h;
	const void* image = nk_font_atlas_bake(&atlas, &img_w, &img_h, NK_FONT_ATLAS_RGBA32);

  size_t bakedSize = (size_t)(img_w * img_h * 4);
  std::vector<uint8_t> imageCopy(bakedSize);
  memcpy(imageCopy.data(), image, bakedSize);

  struct nk_draw_null_texture nullTex;
	nk_font_atlas_end(&this->atlas, nk_handle_id(0), &nullTex);

  this->scaledFont->handle.height = BASE_FONT_SIZE * this->uiScale;
	nk_init_default(&this->uiContext, &this->scaledFont->handle);

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
    this->CreateBindingLayout(0, wgpu::ShaderStage::Fragment, samplerType),
    this->CreateBindingLayout(1, wgpu::ShaderStage::Fragment),
    this->CreateBindingLayout(2, wgpu::ShaderStage::Vertex, 64),
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
  colorTarget.format = this->context.surfaceFormat;
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

void UIRenderPass::RenderUI()
{
  nk_context* ctx = &this->uiContext; //alias for simpler code

  float s = this->uiScale;
  ctx->style.window.padding          = nk_vec2(4.0f * s, 4.0f * s);
  ctx->style.window.spacing          = nk_vec2(4.0f * s, 4.0f * s);
  ctx->style.window.scrollbar_size   = nk_vec2(10.0f * s, 10.0f * s);
  ctx->style.window.min_row_height_padding = (8.0f * s);
  ctx->style.button.padding          = nk_vec2(4.0f * s, 4.0f * s);
  ctx->style.text.padding            = nk_vec2(4.0f * s, 4.0f * s);
  ctx->style.checkbox.padding        = nk_vec2(2.0f * s, 2.0f * s);
  ctx->style.tab.padding             = nk_vec2(4.0f * s, 4.0f * s);
  ctx->style.tab.indent              = 10.0f * s;
  ctx->style.window.rounding         = 0.0f;

  nk_color bg = nk_rgb(20, 20, 20);
  ctx->style.window.background = bg;
  ctx->style.window.fixed_background = nk_style_item_color(bg);

  static nk_flags flags = NK_WINDOW_BORDER;
  glm::vec2 menu_size(this->context.size);
  menu_size.x /= 4;

  //main side window
  if (nk_begin(ctx, "Test Window", nk_rect(0, 0, menu_size.x, menu_size.y), flags))
  {
    nk_layout_row_dynamic(ctx, 0, 2);
    
    nk_label(ctx, this->current_filename.c_str(), NK_TEXT_LEFT);
    if (nk_button_label(ctx, "Open File"))
      utils::OpenFile("BezierView File", "bv", [this](std::string s){Settings::parser.Parse(s);});

    if (nk_checkbox_label(ctx, "Enable Tessellation", (nk_bool*)&Settings::tessellation.get()))
      Settings::tessellation.mark();

    nk_spacer(ctx);

    if (nk_tree_push(ctx, NK_TREE_TAB, "Object Properties", NK_MINIMIZED))
    {
      nk_layout_row_dynamic(ctx, 0, 2);

      nk_label(ctx, "Translation", NK_TEXT_RIGHT);
      nk_property_float(ctx, "Trans X:", -1000.0f, &Settings::mvp.get().translation.x, 1000.0f, 0.01f, 0.01f);
      nk_spacer(ctx);
      nk_property_float(ctx, "Trans Y:", -1000.0f, &Settings::mvp.get().translation.y, 1000.0f, 0.01f, 0.01f);
      nk_spacer(ctx);
      nk_property_float(ctx, "Trans Z:", -1000.0f, &Settings::mvp.get().translation.z, 1000.0f, 0.01f, 0.01f);

      nk_layout_row_static(ctx, 10, 0, 1);
      nk_spacer(ctx);
      nk_layout_row_dynamic(ctx, 0, 2);

      nk_label(ctx, "Rotation (degrees)", NK_TEXT_RIGHT);
      nk_property_float(ctx, "Rot X:", -360.0f, &Settings::mvp.get().rotation.x, 360.0f, 1.0f, 0.5f);
      nk_spacer(ctx);
      nk_property_float(ctx, "Rot Y:", -360.0f, &Settings::mvp.get().rotation.y, 360.0f, 1.0f, 0.5f);
      nk_spacer(ctx);
      nk_property_float(ctx, "Rot Z:", -360.0f, &Settings::mvp.get().rotation.z, 360.0f, 1.0f, 0.5f);

      nk_layout_row_static(ctx, 10, 0, 1);
      nk_spacer(ctx);
      nk_layout_row_dynamic(ctx, 0, 2);

      nk_label(ctx, "Scale", NK_TEXT_RIGHT);
      nk_property_float(ctx, "Scale X:", 0.001f, &Settings::mvp.get().scale.x, 1000.0f, 0.05f, 0.01f);
      nk_spacer(ctx);
      nk_property_float(ctx, "Scale Y:", 0.001f, &Settings::mvp.get().scale.y, 1000.0f, 0.05f, 0.01f);
      nk_spacer(ctx);
      nk_property_float(ctx, "Scale Z:", 0.001f, &Settings::mvp.get().scale.z, 1000.0f, 0.05f, 0.01f);

      nk_layout_row_static(ctx, 10, 0, 1);
      nk_spacer(ctx);

      nk_layout_row_static(ctx, 0, this->context.size.x / 10, 1);

      if (nk_button_label(ctx, "Reset Transform"))
      {
        Settings::mvp.get().translation = { 0.0f, 0.0f, 0.0f };
        Settings::mvp.get().rotation    = { 0.0f, 0.0f, 0.0f };
        Settings::mvp.get().scale       = { 1.0f, 1.0f, 1.0f };
      }

      nk_layout_row_static(ctx, 10, 0, 1);
      nk_spacer(ctx);

      nk_tree_pop(ctx);
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Profiling", NK_MINIMIZED))
    {
      if (nk_checkbox_label(ctx, "Show Performance Window", (nk_bool*)&Settings::perfWindow.get()))
        Settings::perfWindow.mark();
      //extra settings, what to measure, etc.
      nk_tree_pop(ctx);
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Settings", NK_MINIMIZED))
    {
      nk_layout_row_dynamic(ctx, 0, 1);
      if (nk_checkbox_label(ctx, "Wireframe", (nk_bool*)&Settings::wireframe.get()))
        Settings::wireframe.mark();
     
      nk_label(ctx, "Background Color", NK_TEXT_LEFT);

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

      nk_tree_pop(ctx);
    }

  }
  nk_end(&this->uiContext);

  if (Settings::perfWindow.get())
  {
    char fpsBuf[64];
    std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", this->context.performance.fps);

    char ftBuf[64];
    std::snprintf(ftBuf, sizeof(ftBuf), "Frame: %.2f ms", this->context.performance.avg_frametime);

    const auto& font = *ctx->style.font;
    const auto& win  = ctx->style.window;

    const char* widthTemplate = "FPS: 0000.0";
    float perf_width = font.width(font.userdata, font.height, widthTemplate, (int)strlen(widthTemplate)) * 2.0f;

    float row_height    = font.height * 1.1f;
    float header_height = font.height + win.header.padding.y * 2.0f + win.header.label_padding.y * 2.0f;
    float perf_height   = header_height + row_height * 3.0f;

    float margin = 8.0f * s;
    float perf_x  = (float)this->context.size.x - perf_width - margin;

    if (this->screenResized)
    {
      nk_window_set_bounds(ctx, "Performance", nk_rect(perf_x, margin, perf_width, perf_height));
      this->screenResized = false;
    }

    nk_flags perfFlags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE;
    if (nk_begin(ctx, "Performance", nk_rect(perf_x, margin, perf_width, perf_height), perfFlags))
    {
      nk_layout_row_static(ctx, row_height, (int)perf_width, 1);
      nk_label(ctx, fpsBuf, NK_TEXT_LEFT);
      nk_label(ctx, ftBuf, NK_TEXT_LEFT);
    }
    nk_end(ctx);
  }

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