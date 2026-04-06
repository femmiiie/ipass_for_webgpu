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
    if (rows < 2 || cols < 2) continue; // skip degenerate patches

    auto getVert = [&](glm::u32 r, glm::u32 c) -> const utils::Vertex3D& {
      return patch[r * cols + c];
    };

    // convert quad cell to 2 tris
    for (glm::u32 i = 0; i + 1 < rows; i++)
    {
      for (glm::u32 j = 0; j + 1 < cols; j++)
      {
        const utils::Vertex3D& tl = getVert(i,     j);
        const utils::Vertex3D& tr = getVert(i,     j + 1);
        const utils::Vertex3D& bl = getVert(i + 1, j);
        const utils::Vertex3D& br = getVert(i + 1, j + 1);

        glm::vec3 ptl = glm::vec3(tl.pos);
        glm::vec3 ptr_ = glm::vec3(tr.pos);
        glm::vec3 pbl = glm::vec3(bl.pos);
        glm::vec3 pbr = glm::vec3(br.pos);

        // tri 1: tl, tr, br - tri 2: tl, br, bl
        glm::vec3 e1a = ptr_ - ptl, e1b = pbr - ptl;
        glm::vec3 n1 = glm::cross(e1a, e1b);
        if (glm::length(n1) > 0.0f) n1 = glm::normalize(n1);

        glm::vec3 e2a = pbr - ptl, e2b = pbl - ptl;
        glm::vec3 n2 = glm::cross(e2a, e2b);
        if (glm::length(n2) > 0.0f) n2 = glm::normalize(n2);

        // pos(xyzw) normal(xyzw) color(rgba) uv(uv) pad(00) - 16f size
        auto pushVert = [&](const utils::Vertex3D& v, const glm::vec3& normal) {
          vertexData.insert(vertexData.end(), {
            v.pos.x,   v.pos.y,   v.pos.z,   v.pos.w,
            normal.x,  normal.y,  normal.z,  0.0f,
            v.color.r, v.color.g, v.color.b, v.color.a,
            v.tex.x,   v.tex.y,   0.0f,      0.0f
          });
        };

        pushVert(tl, n1); pushVert(tr, n1); pushVert(br, n1); // tri 1
        pushVert(tl, n2); pushVert(br, n2); pushVert(bl, n2); // tri 2
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

  this->light = {
    .position    = glm::vec4(-1.0f, 2.0f, 0.0f, 1.0f),
    .color       = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .power       = 1.0f,
    .shadingMode = 0u
  };

  const wgpu::BufferUsage uniformUsage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

  this->bindGroupLayout = this->CreateBindGroupLayout({
    this->CreateBufferLayout(0, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get().data)),
    this->CreateBufferLayout(1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, utils::aligned_size(this->light)),
  });

  this->mvpBuffer   = this->CreateBuffer(utils::aligned_size(Settings::mvp.get().data), uniformUsage);
  this->lightBuffer = this->CreateBuffer(utils::aligned_size(this->light), uniformUsage);

  this->bindGroup = this->CreateBindGroup({
    this->CreateBinding(0, this->mvpBuffer),
    this->CreateBinding(1, this->lightBuffer)
  });

  this->context.queue.writeBuffer(this->mvpBuffer, 0, &Settings::mvp.get().data, sizeof(MVP::GPUData));
  this->context.queue.writeBuffer(this->lightBuffer, 0, &this->light, sizeof(Light));

  Settings::mvp.subscribe([this](const MVP& m) {
    this->context.queue.writeBuffer(this->mvpBuffer, 0, &m.data, sizeof(MVP::GPUData));
  });

  Settings::parser.subscribe([this](const BVParser& p) {
    this->LoadBV(p);
  });

  Settings::tessOutput.subscribe([this](const TessOutput& out) {
    if (Settings::tessellation.get() && out.buffer && out.vertexCount > 0)
      this->UseGPUTessellated(out.buffer, out.vertexCount);
  });

  Settings::tessellation.subscribe([this](const bool& enabled) {
    const auto& out = Settings::tessOutput.get();
    if (enabled && out.buffer && out.vertexCount > 0)
      this->UseGPUTessellated(out.buffer, out.vertexCount);
    else
      this->LoadBV(Settings::parser.get());
  });

  Settings::shadingMode.subscribe([this](const ShadingMode& mode) {
    this->light.shadingMode = static_cast<uint32_t>(mode);
    this->context.queue.writeBuffer(this->lightBuffer, 0, &this->light, sizeof(Light));
  });

  this->InitializeRenderPipeline();
}

SceneRenderPass::~SceneRenderPass()
{
  this->pipeline.release();
  this->wireframePipeline.release();
  this->layout.release();
  this->bindGroupLayout.release();
  this->bindGroup.release();
  if (this->depthTextureView) this->depthTextureView.release();
  if (this->depthTexture)     this->depthTexture.destroy();
  if (this->wireframeIndexBuffer) this->wireframeIndexBuffer.destroy();
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

  encoder.setVertexBuffer(0, this->vertexBuffer, 0, this->vertexBuffer.getSize());
  encoder.setBindGroup(0, this->bindGroup, 0, nullptr);

  if (!Settings::tessellation.get() && Settings::wireframe.get())
  {
    encoder.setPipeline(this->wireframePipeline);
    encoder.setIndexBuffer(this->wireframeIndexBuffer, wgpu::IndexFormat::Uint32, 0, this->wireframeIndexBuffer.getSize());
    encoder.drawIndexed(this->wireframeIndexCount, 1, 0, 0, 0);
  }
  else
  {
    encoder.setPipeline(this->pipeline);
    encoder.draw(this->vertexCount, 1, 0, 0);
  }
}

void SceneRenderPass::OnResize(glm::uvec2 size)
{
  const Context::Viewport& vp = context.sceneViewport;
  camera.getAspect_M() = (vp.height > 0.0f) ? vp.width / vp.height : 1.0f;
  camera.deferUpdate();
  this->CreateDepthTexture(size);
}

void SceneRenderPass::InitializeRenderPipeline()
{
  wgpu::ShaderModule shaderModule = utils::LoadShader(this->context.device, "Pass/RenderPass/scene.wgsl");
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
    this->CreateAttribute(0, wgpu::VertexFormat::Float32x4, 0),
    this->CreateAttribute(1, wgpu::VertexFormat::Float32x4, 4 * sizeof(glm::f32)),
    this->CreateAttribute(2, wgpu::VertexFormat::Float32x4, 8 * sizeof(glm::f32)),
    this->CreateAttribute(3, wgpu::VertexFormat::Float32x2, 12 * sizeof(glm::f32)),
  };

  wgpu::VertexBufferLayout vertexBufferLayout;
  vertexBufferLayout.attributeCount = vertexAttrs.size();
  vertexBufferLayout.attributes = vertexAttrs.data();
  vertexBufferLayout.arrayStride = 16 * sizeof(glm::f32);
  vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

  pipelineDesc.vertex.bufferCount = 1;
  pipelineDesc.vertex.buffers = &vertexBufferLayout;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  utils::SetEntryPoint(pipelineDesc.vertex.entryPoint, "vs_main");


  pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
  pipelineDesc.primitive.frontFace = wgpu::FrontFace::CW;
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

  wgpu::DepthStencilState depthStencilState = wgpu::Default;
  depthStencilState.depthCompare = wgpu::CompareFunction::Less;
  depthStencilState.depthWriteEnabled = wgpu::OptionalBool::True;
  depthStencilState.format = wgpu::TextureFormat::Depth24Plus;
  depthStencilState.stencilReadMask = 0;
  depthStencilState.stencilWriteMask = 0;

  pipelineDesc.depthStencil = &depthStencilState;
  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  this->pipeline = this->context.device.createRenderPipeline(pipelineDesc);

  // duplicate regular pipeline, just render lines instead of tris
  pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::LineList;
  pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
  this->wireframePipeline = this->context.device.createRenderPipeline(pipelineDesc);

  shaderModule.release();
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