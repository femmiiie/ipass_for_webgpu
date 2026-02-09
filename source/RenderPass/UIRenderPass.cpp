#include "UIRenderPass.h"

#include <cstring>

UIRenderPass::UIRenderPass(RenderContext& context) : RenderPass(context)
{

	nk_font_atlas_init_default(&this->atlas);
	nk_font_atlas_begin(&this->atlas);

	nk_font* default_font = nk_font_atlas_add_default(&atlas, 13.0f, nullptr);

	int img_w, img_h;
	const void* image = nk_font_atlas_bake(&atlas, &img_w, &img_h, NK_FONT_ATLAS_RGBA32);

  struct nk_draw_null_texture nullTex;
	nk_font_atlas_end(&this->atlas, nk_handle_id(0), &nullTex);
	nk_init_default(&this->uiContext, &default_font->handle);

  nk_buffer_init_default(&this->cmds);
  nk_buffer_init_default(&this->verts);
  nk_buffer_init_default(&this->idx);

  static const struct nk_draw_vertex_layout_element vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(Vertex, pos)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(Vertex, tex)},
    {NK_VERTEX_COLOR,    NK_FORMAT_R32G32B32A32_FLOAT, offsetof(Vertex, col)},
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
  this->context.queue.writeTexture(destination, image, imageSize, source, textureDesc.size);

  // Create projection uniform buffer (mat4x4f = 64 bytes)
  wgpu::BufferDescriptor projDesc;
  projDesc.size = 64;
  projDesc.usage = wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform));
  projDesc.mappedAtCreation = false;
  this->projectionBuffer = this->context.device.createBuffer(projDesc);

  this->UpdateProjection(this->context.screenSize);

  // Bind group layout: sampler, texture, projection uniform
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

    // Iterate draw commands and issue indexed draws
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
  wgpu::ShaderModule shaderModule = context.LoadShader("ui.wgsl");
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

  pipelineDesc.depthStencil = nullptr;
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
  this->context.screenSize = size;

  glm::mat4 projection = glm::ortho(0.0f, (float)size.x, (float)size.y, 0.0f, -1.0f, 1.0f);
  this->context.queue.writeBuffer(this->projectionBuffer, 0, &projection, sizeof(projection));
}

void UIRenderPass::RenderUI()
{
  static nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE;
  glm::vec2 menu_size(this->context.screenSize);
  menu_size.x /= 4;
  if (nk_begin(&this->uiContext, "Test Window", nk_rect(0, 0, menu_size.x, menu_size.y), flags))
  {
    nk_layout_row_static(&this->uiContext, 30, 80, 1);
    if (nk_button_label(&this->uiContext, "Click me"))
    {
      std::cout << "Button was pressed!" << std::endl;
    }

    nk_layout_row_dynamic(&this->uiContext, 20, 1);
    nk_label(&this->uiContext, "Nuklear is working!", NK_TEXT_LEFT);
  }
  nk_end(&this->uiContext);

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