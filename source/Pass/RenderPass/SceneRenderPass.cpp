#include "SceneRenderPass.h"
#include "Settings.h"

#include <cmath>

void SceneRenderPass::LoadBV(const BVParser& parser)
{
  const std::vector<Patch>& patches = parser.Get();
  const std::vector<std::pair<glm::u32,glm::u32>>& dims = parser.GetDims();

  std::vector<glm::f32> vertexData;

  for (size_t patchIdx = 0; patchIdx < patches.size(); patchIdx++)
  {
    const Patch& patch = patches[patchIdx];
    if (patch.empty()) continue;

    glm::u32 rows = dims[patchIdx].first;
    glm::u32 cols = dims[patchIdx].second;
    if (rows < 2 || cols < 2) continue;

    auto getVert = [&](glm::u32 r, glm::u32 c) -> const utils::Vertex3D& {
      return patch[r * cols + c];
    };

    for (glm::u32 i = 0; i + 1 < rows; i++)
    {
      for (glm::u32 j = 0; j + 1 < cols; j++)
      {
        const utils::Vertex3D& tl = getVert(i,     j);
        const utils::Vertex3D& tr = getVert(i,     j + 1);
        const utils::Vertex3D& bl = getVert(i + 1, j);
        const utils::Vertex3D& br = getVert(i + 1, j + 1);

        auto project = [](const glm::vec4& p) -> glm::vec3 {
          float w = std::abs(p.w) > 1e-7f ? p.w : 1.0f;
          return glm::vec3(p) / w;
        };

        glm::vec3 n = glm::cross(
          project(bl.pos) - project(tl.pos),
          project(tr.pos) - project(tl.pos)
        );
        if (glm::length(n) > 0.0f) n = glm::normalize(n);

        // pos(xyzw) normal(xyzw) color(rgba) uv(uv) patch_index, bary_id - 16f size
        auto pushVert = [&](const utils::Vertex3D& v, float bary_id) {
          float w = std::abs(v.pos.w) > 1e-7f ? v.pos.w : 1.0f;
          glm::vec4 pos = v.pos / w;
          vertexData.insert(vertexData.end(), {
            pos.x,     pos.y,   pos.z,   1.0f,
            n.x,       n.y,     n.z,     0.0f,
            v.color.r, v.color.g, v.color.b, v.color.a,
            v.tex.x,   v.tex.y,   0.0f,      bary_id
          });
        };

        pushVert(tl, 0.0f); pushVert(tr, 1.0f); pushVert(br, 2.0f); // tri 1
        pushVert(tl, 0.0f); pushVert(br, 1.0f); pushVert(bl, 2.0f); // tri 2
      }
    }
  }

  if (vertexData.empty())
  {
    std::cerr << "[LoadBV] No vertex data generated (no valid patches)." << std::endl;
    this->vertexCount = 0;
    return;
  }

  this->vertexCount = (glm::u32)vertexData.size() / 16;

  if (this->vertexBuffer && this->ownsVertexBuffer)
    this->vertexBuffer.destroy();
  this->vertexBuffer = nullptr;
  this->ownsVertexBuffer = true;

  this->vertexBuffer = this->CreateBuffer(
    vertexData.size() * sizeof(glm::f32),
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex)),
    false
  );

  this->context.queue.writeBuffer(this->vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(glm::f32));

  glm::u32 numTriangles = this->vertexCount / 3;
  std::vector<glm::u32> wireframeIndices;
  wireframeIndices.reserve(numTriangles * 6);
  for (glm::u32 i = 0; i < numTriangles; i++)
  {
    glm::u32 base = i * 3;
    wireframeIndices.push_back(base + 0);
    wireframeIndices.push_back(base + 1);
    wireframeIndices.push_back(base + 1);
    wireframeIndices.push_back(base + 2);
    wireframeIndices.push_back(base + 2);
    wireframeIndices.push_back(base + 0);
  }
  this->wireframeIndexCount = (glm::u32)wireframeIndices.size();

  if (this->wireframeIndexBuffer)
    this->wireframeIndexBuffer.destroy();

  this->wireframeIndexBuffer = this->CreateBuffer(
    wireframeIndices.size() * sizeof(glm::u32),
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index)),
    false
  );
  this->context.queue.writeBuffer(this->wireframeIndexBuffer, 0, wireframeIndices.data(), wireframeIndices.size() * sizeof(glm::u32));

  std::cout << "[LoadBV] Generated " << this->vertexCount << " vertices from " << patches.size() << " patches." << std::endl;
}

SceneRenderPass::SceneRenderPass(Context& context) : RenderPass(context)
{
  camera.setScrollScaling(0.5f);
  float vp_width = context.sceneViewport.width;
  float vp_height = context.sceneViewport.height;
  camera.getAspect_M() = (vp_height > 0.0f) ? vp_width / vp_height : 1.0f;
  camera.deferUpdate();

  InputManager::AddScrollCallback(
    [this](double x, double y) { camera.OnScroll(x, y); }
  );
  InputManager::AddMouseButtonCallback(
    GLFW_MOUSE_BUTTON_MIDDLE,
    [this](int action, int mods) { camera.OnMouseButton(action, mods); }
  );

  this->LoadBV(Settings::parser.get());
  this->CreateDepthTexture(context.size);

  Settings::mvp.modify().setModel();
  Settings::mvp.modify().setLookAt(glm::vec3(0, -2, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
  Settings::mvp.modify().setPerspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.0f);

  this->lightData = {
    .position    = glm::vec4(-1.0f, 2.0f, 0.0f, 1.0f),
    .color       = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .power       = 1.0f,
  };

  this->viewportData = {
    .x      = context.sceneViewport.x,
    .y      = context.sceneViewport.y,
    .width  = context.sceneViewport.width,
    .height = context.sceneViewport.height,
  };

  const wgpu::BufferUsage uniformUsage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

  this->mvpBuffer      = this->CreateBuffer(utils::aligned_size(Settings::mvp.get().data), uniformUsage);
  this->lightBuffer    = this->CreateBuffer(utils::aligned_size(this->lightData), uniformUsage);
  this->viewportBuffer = this->CreateBuffer(utils::aligned_size(this->viewportData), uniformUsage);

  this->controlPointsBuffer = this->CreateBuffer( // dummy initial buffer
    16 * sizeof(glm::vec4),
    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst
  );
  this->ownsControlPointsBuffer = true;

  this->context.queue.writeBuffer(this->mvpBuffer,      0, &Settings::mvp.get().data, sizeof(MVP::GPUData));
  this->context.queue.writeBuffer(this->lightBuffer,    0, &this->lightData,   sizeof(LightData));
  this->context.queue.writeBuffer(this->viewportBuffer, 0, &this->viewportData, sizeof(ViewportData));

  Settings::mvp.subscribe([this](const MVP& m) {
    this->context.queue.writeBuffer(this->mvpBuffer, 0, &m.data, sizeof(MVP::GPUData));
  });

  Settings::parser.subscribe([this](const BVParser& p) {
    this->LoadBV(p);
  });

  Settings::tessOutput.subscribe([this](const TessOutput& out) {
    if (Settings::tessellation.get() && out.buffer && out.vertexCount > 0)
      this->UseGPUTessellated(out.buffer, out.vertexCount);
    if (out.controlPoints) {
      if (this->ownsControlPointsBuffer && this->controlPointsBuffer)
        this->controlPointsBuffer.destroy();
      this->controlPointsBuffer = out.controlPoints;
      this->ownsControlPointsBuffer = false;
      this->RebuildParametricErrorBindGroup();
    }
  });

  Settings::tessellation.subscribe([this](const bool& enabled) {
    const auto& out = Settings::tessOutput.get();
    if (enabled && out.buffer && out.vertexCount > 0)
      this->UseGPUTessellated(out.buffer, out.vertexCount);
    else
      this->LoadBV(Settings::parser.get());
  });

  Settings::shadingMode.subscribe([this](const ShadingMode& mode) {
    this->activeMode = mode;
  });

  this->InitializeShaderVariants();
}

SceneRenderPass::~SceneRenderPass()
{
  for (auto& v : this->shaderVariants)
  {
    if (v.pipeline)          v.pipeline.release();
    if (v.wireframePipeline) v.wireframePipeline.release();
    if (v.layout)            v.layout.release();
    if (v.bindGroupLayout)   v.bindGroupLayout.release();
    if (v.bindGroup)         v.bindGroup.release();
  }

  if (this->depthTextureView) this->depthTextureView.release();
  if (this->depthTexture)     this->depthTexture.destroy();
  if (this->wireframeIndexBuffer) this->wireframeIndexBuffer.destroy();
  if (this->ownsControlPointsBuffer && this->controlPointsBuffer) this->controlPointsBuffer.destroy();
}


void SceneRenderPass::Execute(wgpu::RenderPassEncoder& encoder)
{
  const Context::Viewport& vp = context.sceneViewport;
  encoder.setViewport(vp.x, vp.y, vp.width, vp.height, 0.0f, 1.0f);
  encoder.setScissorRect((uint32_t)vp.x, (uint32_t)vp.y,
                         (uint32_t)vp.width, (uint32_t)vp.height);

  if (camera.requiresUpdate())
  {
    camera.update();
    Settings::mvp.modify().setProjection(camera.getProjectionMatrix());
  }

  if (camera.consumeViewUpdate())
    Settings::mvp.modify().setView(camera.getViewMatrix());

  Settings::mvp.notify();

  if (!this->vertexBuffer || this->vertexCount == 0) return;

  auto& variant = this->shaderVariants[static_cast<size_t>(this->activeMode)];
  encoder.setVertexBuffer(0, this->vertexBuffer, 0, this->vertexBuffer.getSize());
  encoder.setBindGroup(0, variant.bindGroup, 0, nullptr);

  if (!Settings::tessellation.get() && Settings::wireframe.get())
  {
    encoder.setPipeline(variant.wireframePipeline);
    encoder.setIndexBuffer(this->wireframeIndexBuffer, wgpu::IndexFormat::Uint32, 0, this->wireframeIndexBuffer.getSize());
    encoder.drawIndexed(this->wireframeIndexCount, 1, 0, 0, 0);
  }
  else
  {
    encoder.setPipeline(variant.pipeline);
    encoder.draw(this->vertexCount, 1, 0, 0);
  }
}

void SceneRenderPass::OnResize(glm::uvec2 size)
{
  const Context::Viewport& vp = context.sceneViewport;
  camera.getAspect_M() = (vp.height > 0.0f) ? vp.width / vp.height : 1.0f;
  camera.deferUpdate();
  this->CreateDepthTexture(size);

  this->viewportData.x      = vp.x;
  this->viewportData.y      = vp.y;
  this->viewportData.width  = vp.width;
  this->viewportData.height = vp.height;
  this->context.queue.writeBuffer(this->viewportBuffer, 0, &this->viewportData, sizeof(ViewportData));
}

wgpu::RenderPipeline SceneRenderPass::CreatePipeline(wgpu::ShaderModule& shader,
                                                     wgpu::PipelineLayout& pipelineLayout,
                                                     wgpu::PrimitiveTopology topology)
{
  std::vector<wgpu::VertexAttribute> vertexAttrs {
    this->CreateAttribute(0, wgpu::VertexFormat::Float32x4, 0),
    this->CreateAttribute(1, wgpu::VertexFormat::Float32x4, 4 * sizeof(glm::f32)),
    this->CreateAttribute(2, wgpu::VertexFormat::Float32x4, 8 * sizeof(glm::f32)),
    this->CreateAttribute(3, wgpu::VertexFormat::Float32x2, 12 * sizeof(glm::f32)),
    this->CreateAttribute(4, wgpu::VertexFormat::Float32,   14 * sizeof(glm::f32)),
    this->CreateAttribute(5, wgpu::VertexFormat::Float32,   15 * sizeof(glm::f32)),
  };

  wgpu::VertexBufferLayout vertexBufferLayout;
  vertexBufferLayout.attributeCount = vertexAttrs.size();
  vertexBufferLayout.attributes     = vertexAttrs.data();
  vertexBufferLayout.arrayStride    = 16 * sizeof(glm::f32);
  vertexBufferLayout.stepMode       = wgpu::VertexStepMode::Vertex;

  wgpu::BlendState blend = this->GetBlendState();

  wgpu::ColorTargetState colorTarget;
  colorTarget.format    = this->context.colorFormat;
  colorTarget.blend     = &blend;
  colorTarget.writeMask = wgpu::ColorWriteMask::All;

  wgpu::FragmentState fragmentState;
  fragmentState.module        = shader;
  fragmentState.constantCount = 0;
  fragmentState.constants     = nullptr;
  fragmentState.targetCount   = 1;
  fragmentState.targets       = &colorTarget;
  utils::SetEntryPoint(fragmentState.entryPoint, "fs_main");

  wgpu::DepthStencilState depthStencilState = wgpu::Default;
  depthStencilState.depthCompare            = wgpu::CompareFunction::Less;
  depthStencilState.depthWriteEnabled       = wgpu::OptionalBool::True;
  depthStencilState.format                  = wgpu::TextureFormat::Depth24Plus;
  depthStencilState.stencilReadMask         = 0;
  depthStencilState.stencilWriteMask        = 0;

  wgpu::RenderPipelineDescriptor pipelineDesc;
  pipelineDesc.layout = pipelineLayout;

  pipelineDesc.vertex.bufferCount  = 1;
  pipelineDesc.vertex.buffers      = &vertexBufferLayout;
  pipelineDesc.vertex.module       = shader;
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants    = nullptr;
  utils::SetEntryPoint(pipelineDesc.vertex.entryPoint, "vs_main");

  pipelineDesc.primitive.topology         = topology;
  pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
  pipelineDesc.primitive.frontFace        = wgpu::FrontFace::CW;
  pipelineDesc.primitive.cullMode         = wgpu::CullMode::None;

  pipelineDesc.fragment          = &fragmentState;
  pipelineDesc.depthStencil      = &depthStencilState;
  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask  = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  return this->context.device.createRenderPipeline(pipelineDesc);
}

void SceneRenderPass::InitializeShaderVariants()
{
  const wgpu::ShaderStage vsfs = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  const wgpu::ShaderStage fs   = wgpu::ShaderStage::Fragment;

  { //blinn-phong shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::BlinnPhong)];

    v.bindGroupLayout = this->CreateBindGroupLayout({
      this->CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get().data)),
      this->CreateBufferLayout(1, fs,   wgpu::BufferBindingType::Uniform, utils::aligned_size(this->lightData)),
    });

    v.bindGroup = this->CreateBindGroup(
      std::vector<wgpu::BindGroupEntry>{
        this->CreateBinding(0, this->mvpBuffer),
        this->CreateBinding(1, this->lightBuffer),
      }, v.bindGroupLayout);

    wgpu::PipelineLayoutDescriptor layoutDesc;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&v.bindGroupLayout;
    v.layout = this->context.device.createPipelineLayout(layoutDesc);

    wgpu::ShaderModule shader = utils::LoadShader(this->context.device,
                                  "Pass/RenderPass/shaders/blinn_phong.wgsl");
    if (!shader) throw new RenderPassException("Failed to load blinn_phong.wgsl");
    v.pipeline          = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::TriangleList);
    v.wireframePipeline = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::LineList);
    shader.release();
  }

  { //flat shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::Flat)];

    v.bindGroupLayout = this->CreateBindGroupLayout({
      this->CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get().data)),
    });

    v.bindGroup = this->CreateBindGroup(
      std::vector<wgpu::BindGroupEntry>{
        this->CreateBinding(0, this->mvpBuffer),
      }, v.bindGroupLayout);

    wgpu::PipelineLayoutDescriptor layoutDesc;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&v.bindGroupLayout;
    v.layout = this->context.device.createPipelineLayout(layoutDesc);

    wgpu::ShaderModule shader = utils::LoadShader(this->context.device,
                                  "Pass/RenderPass/shaders/flat.wgsl");
    if (!shader) throw new RenderPassException("Failed to load flat.wgsl");
    v.pipeline          = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::TriangleList);
    v.wireframePipeline = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::LineList);
    shader.release();
  }

  { //parametric error shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::ParametricError)];

    v.bindGroupLayout = this->CreateBindGroupLayout({
      this->CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform,         utils::aligned_size(Settings::mvp.get().data)),
      this->CreateBufferLayout(1, fs,   wgpu::BufferBindingType::Uniform,         utils::aligned_size(this->viewportData)),
      this->CreateBufferLayout(2, fs,   wgpu::BufferBindingType::ReadOnlyStorage, 0),
    });

    v.bindGroup = this->CreateBindGroup(
      std::vector<wgpu::BindGroupEntry>{
        this->CreateBinding(0, this->mvpBuffer),
        this->CreateBinding(1, this->viewportBuffer),
        this->CreateBinding(2, this->controlPointsBuffer),
      }, v.bindGroupLayout);

    wgpu::PipelineLayoutDescriptor layoutDesc;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&v.bindGroupLayout;
    v.layout = this->context.device.createPipelineLayout(layoutDesc);

    wgpu::ShaderModule shader = utils::LoadShader(this->context.device,
                                  "Pass/RenderPass/shaders/parametric_error.wgsl");
    if (!shader) throw new RenderPassException("Failed to load parametric_error.wgsl");
    v.pipeline          = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::TriangleList);
    v.wireframePipeline = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::LineList);
    shader.release();
  }

  { //triangle size shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::TriangleSize)];

    v.bindGroupLayout = this->CreateBindGroupLayout({
      this->CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get().data)),
    });

    v.bindGroup = this->CreateBindGroup(
      std::vector<wgpu::BindGroupEntry>{
        this->CreateBinding(0, this->mvpBuffer),
      }, v.bindGroupLayout);

    wgpu::PipelineLayoutDescriptor layoutDesc;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&v.bindGroupLayout;
    v.layout = this->context.device.createPipelineLayout(layoutDesc);

    wgpu::ShaderModule shader = utils::LoadShader(this->context.device,
                                  "Pass/RenderPass/shaders/triangle_size.wgsl");
    if (!shader) throw new RenderPassException("Failed to load triangle_size.wgsl");
    v.pipeline          = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::TriangleList);
    v.wireframePipeline = this->CreatePipeline(shader, v.layout, wgpu::PrimitiveTopology::LineList);
    shader.release();
  }
}

wgpu::VertexAttribute SceneRenderPass::CreateAttribute(glm::u32 location, wgpu::VertexFormat format, uint64_t offset)
{
  wgpu::VertexAttribute attr;
  attr.shaderLocation = location;
  attr.format = format;
  attr.offset = offset;
  return attr;
}

wgpu::BlendState SceneRenderPass::GetBlendState()
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

void SceneRenderPass::UseGPUTessellated(wgpu::Buffer buf, uint32_t count)
{
  if (this->vertexBuffer && this->ownsVertexBuffer)
    this->vertexBuffer.destroy();

  this->ownsVertexBuffer = false;
  this->vertexBuffer    = buf;

  const uint64_t stride = 16ull * sizeof(glm::f32);
  const uint64_t maxVertsByBuffer = stride > 0 ? (buf.getSize() / stride) : 0;
  this->vertexCount = static_cast<glm::u32>(std::min<uint64_t>(count, maxVertsByBuffer));

  if (this->vertexCount < count)
  {
    std::cerr << "[UseGPUTessellated] Clamped draw vertex count from " << count
              << " to " << this->vertexCount
              << " to fit the bound vertex buffer capacity." << std::endl;
  }
}

void SceneRenderPass::CreateDepthTexture(glm::uvec2 size)
{
  if (this->depthTextureView) this->depthTextureView.release();
  if (this->depthTexture)     this->depthTexture.destroy();

  wgpu::TextureDescriptor textureDesc;
  textureDesc.label          = WGPU_STRING_VIEW_INIT;
  textureDesc.usage          = wgpu::TextureUsage::RenderAttachment;
  textureDesc.dimension      = wgpu::TextureDimension::_2D;
  textureDesc.size           = { size.x, size.y, 1 };
  textureDesc.format         = wgpu::TextureFormat::Depth24Plus;
  textureDesc.mipLevelCount  = 1;
  textureDesc.sampleCount    = 1;
  textureDesc.viewFormatCount = 0;
  textureDesc.viewFormats    = nullptr;
  this->depthTexture = this->context.device.createTexture(textureDesc);

  wgpu::TextureViewDescriptor viewDesc;
  viewDesc.label           = WGPU_STRING_VIEW_INIT;
  viewDesc.format          = wgpu::TextureFormat::Depth24Plus;
  viewDesc.dimension       = wgpu::TextureViewDimension::_2D;
  viewDesc.baseMipLevel    = 0;
  viewDesc.mipLevelCount   = 1;
  viewDesc.baseArrayLayer  = 0;
  viewDesc.arrayLayerCount = 1;
  viewDesc.aspect          = wgpu::TextureAspect::DepthOnly;
  this->depthTextureView = this->depthTexture.createView(viewDesc);
}

void SceneRenderPass::RebuildParametricErrorBindGroup()
{
  auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::ParametricError)];
  if (v.bindGroup) v.bindGroup.release();
  v.bindGroup = this->CreateBindGroup(
    std::vector<wgpu::BindGroupEntry>{
      this->CreateBinding(0, this->mvpBuffer),
      this->CreateBinding(1, this->viewportBuffer),
      this->CreateBinding(2, this->controlPointsBuffer),
    }, v.bindGroupLayout);
}