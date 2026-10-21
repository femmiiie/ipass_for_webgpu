#include "ScenePass.h"
#include "GPU.h"
#include "Shader.h"
#include "Settings.h"

#include <cmath>
#include <stdexcept>

namespace
{

}

void ScenePass::LoadPatches(const ipass::PatchData& data)
{
  constexpr glm::u32 ROWS = 4, COLS = 4; // PatchData control points are always bicubic

  std::vector<glm::f32> vertexData;

  for (glm::u32 patchIdx = 0; patchIdx < data.num_patches; patchIdx++)
  {
    const glm::vec4* patch = &data.control_points[patchIdx * 16];

    auto getPos = [&](glm::u32 r, glm::u32 c) -> const glm::vec4& {
      return patch[r * COLS + c];
    };

    for (glm::u32 i = 0; i + 1 < ROWS; i++)
    {
      for (glm::u32 j = 0; j + 1 < COLS; j++)
      {
        const glm::vec4& tl = getPos(i,     j);
        const glm::vec4& tr = getPos(i,     j + 1);
        const glm::vec4& bl = getPos(i + 1, j);
        const glm::vec4& br = getPos(i + 1, j + 1);

        auto project = [](const glm::vec4& p) -> glm::vec3 {
          float w = std::abs(p.w) > 1e-7f ? p.w : 1.0f;
          return glm::vec3(p) / w;
        };

        glm::vec3 n = glm::cross(
          project(bl) - project(tl),
          project(tr) - project(tl)
        );
        if (glm::length(n) > 0.0f) n = glm::normalize(n);

        // pos(xyzw) normal(xyzw) uv(uv) patch_index, bary_id - 12f size
        auto pushVert = [&](const glm::vec4& p, float bary_id) {
          float w = std::abs(p.w) > 1e-7f ? p.w : 1.0f;
          glm::vec4 pos = p / w;
          vertexData.insert(vertexData.end(), {
            pos.x, pos.y, pos.z, 1.0f,
            n.x,   n.y,   n.z,   0.0f,
            1.0f,  1.0f,  0.0f,  bary_id
          });
        };

        pushVert(tl, 0.0f); pushVert(tr, 1.0f); pushVert(br, 2.0f); // tri 1
        pushVert(tl, 0.0f); pushVert(br, 1.0f); pushVert(bl, 2.0f); // tri 2
      }
    }
  }

  if (vertexData.empty())
  {
    std::cerr << "[LoadPatches] No vertex data generated (no valid patches)." << std::endl;
    this->vertexCount = 0;
    return;
  }

  this->vertexCount = (glm::u32)vertexData.size() / 12;

  if (this->vertexBuffer)
    this->vertexBuffer.destroy();

  this->vertexBuffer = utils::CreateBuffer(
    this->context.device,
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

  this->wireframeIndexBuffer = utils::CreateBuffer(
    this->context.device,
    wireframeIndices.size() * sizeof(glm::u32),
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index)),
    false
  );
  this->context.queue.writeBuffer(this->wireframeIndexBuffer, 0, wireframeIndices.data(), wireframeIndices.size() * sizeof(glm::u32));

  std::cout << "[LoadPatches] Generated " << this->vertexCount << " vertices from " << data.num_patches << " patches." << std::endl;
}

ScenePass::ScenePass(Context& context, ipass::ComputeTessPass& computeTessPass, ipass::VertexTessPass& vertexTessPass)
  : context(context), computeTessPass(computeTessPass), vertexTessPass(vertexTessPass)
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

  this->LoadPatches(Settings::patches.get());
  this->CreateDepthTexture(context.size);

  this->lightsData.lights[0] = { glm::vec4(-4.0f, 4.0f, -4.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f };
  this->lightsData.lights[1] = { glm::vec4( 4.0f, 4.0f, -4.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f };
  this->lightsData.lights[2] = { glm::vec4(-4.0f, 4.0f,  4.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f };
  this->lightsData.lights[3] = { glm::vec4( 4.0f, 4.0f,  4.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f };

  this->viewportData = {
    .x      = context.sceneViewport.x,
    .y      = context.sceneViewport.y,
    .width  = context.sceneViewport.width,
    .height = context.sceneViewport.height,
  };

  const wgpu::BufferUsage uniformUsage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

  this->mvpBuffer      = utils::CreateBuffer(this->context.device, utils::aligned_size(Settings::mvp.get()), uniformUsage);
  this->lightBuffer    = utils::CreateBuffer(this->context.device, utils::aligned_size(this->lightsData), uniformUsage);
  this->viewportBuffer = utils::CreateBuffer(this->context.device, utils::aligned_size(this->viewportData), uniformUsage);
  this->colorBuffer    = utils::CreateBuffer(this->context.device, utils::aligned_size(glm::vec4()), uniformUsage);

  this->controlPointsBuffer = utils::CreateBuffer( // dummy initial buffer
    this->context.device,
    16 * sizeof(glm::vec4),
    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst
  );
  this->ownsControlPointsBuffer = true;

  const glm::vec4 defaultBaseColor(0.8f, 0.85f, 0.9f, 1.0f);

  this->context.queue.writeBuffer(this->mvpBuffer,      0, &Settings::mvp.get(), sizeof(MVP));
  this->context.queue.writeBuffer(this->lightBuffer,    0, &this->lightsData,    sizeof(LightsData));
  this->context.queue.writeBuffer(this->viewportBuffer, 0, &this->viewportData,  sizeof(ViewportData));
  this->context.queue.writeBuffer(this->colorBuffer,    0, &defaultBaseColor,    sizeof(glm::vec4));

  Settings::mvp.subscribe([this](const MVP& m) {
    this->context.queue.writeBuffer(this->mvpBuffer, 0, &m, sizeof(MVP));
  });

  Settings::patches.subscribe([this](const ipass::PatchData& p) {
    this->LoadPatches(p);
  });

  Settings::tessOutput.subscribe([this](const TessOutput& out) {
    if (out.controlPoints) {
      if (this->ownsControlPointsBuffer && this->controlPointsBuffer)
        this->controlPointsBuffer.destroy();
      this->controlPointsBuffer = out.controlPoints;
      this->ownsControlPointsBuffer = false;
      this->RebuildParametricErrorBindGroup();
    }
  });

  Settings::shadingMode.subscribe([this](const ShadingMode& mode) {
    this->activeMode = mode;
  });

  this->InitializeShaderVariants();
}

ScenePass::~ScenePass()
{
  for (auto& v : this->shaderVariants)
  {
    if (v.shader)            v.shader.release();
    if (v.pipeline)          v.pipeline.release();
    if (v.wireframePipeline) v.wireframePipeline.release();
    if (v.layout)            v.layout.release();
    if (v.bindGroupLayout)   v.bindGroupLayout.release();
    if (v.bindGroup)         v.bindGroup.release();
    if (v.directPipeline)    v.directPipeline.release();
    if (v.directLayout)      v.directLayout.release();
  }

  if (this->depthTextureView) this->depthTextureView.release();
  if (this->depthTexture)     this->depthTexture.destroy();
  if (this->vertexBuffer)     this->vertexBuffer.destroy();
  if (this->wireframeIndexBuffer) this->wireframeIndexBuffer.destroy();
  if (this->ownsControlPointsBuffer && this->controlPointsBuffer) this->controlPointsBuffer.destroy();
}


void ScenePass::Execute(wgpu::RenderPassEncoder& encoder)
{
  const Context::Viewport& vp = context.sceneViewport;
  encoder.setViewport(vp.x, vp.y, vp.width, vp.height, 0.0f, 1.0f);
  encoder.setScissorRect((uint32_t)vp.x, (uint32_t)vp.y,
                         (uint32_t)vp.width, (uint32_t)vp.height);

  bool projUpdated = camera.requiresUpdate();
  if (projUpdated) camera.update();
  bool viewUpdated = camera.consumeViewUpdate();

  if (projUpdated || viewUpdated)
    Settings::mvp.modify().VP = camera.getProjectionMatrix() * camera.getViewMatrix();
  if (viewUpdated)
    Settings::mvp.modify().cameraPos = glm::vec4(camera.getPosCAR(), 1.0f);

  Settings::mvp.notify();

  auto& variant = this->shaderVariants[static_cast<size_t>(this->activeMode)];
  encoder.setBindGroup(0, variant.bindGroup, 0, nullptr);

  const bool tessellation = Settings::tessellation.get();

  // draw with tessellation in vert shader
  if (tessellation && Settings::directDraw.get())
  {
    encoder.setPipeline(variant.directPipeline);
    this->vertexTessPass.Draw(encoder);
    return;
  }

  // draw taking tessellated output from compute shader
  wgpu::Buffer tessVertexBuffer = this->computeTessPass.GetVertexBuffer();
  if (tessellation && tessVertexBuffer)
  {
    encoder.setPipeline(variant.pipeline);
    encoder.setVertexBuffer(0, tessVertexBuffer, 0, tessVertexBuffer.getSize());
    encoder.draw(this->computeTessPass.GetMaxVertexCount(), 1, 0, 0);
    return;
  }

  // vs_main path: the control mesh
  if (!this->vertexBuffer || this->vertexCount == 0) return;

  encoder.setVertexBuffer(0, this->vertexBuffer, 0, this->vertexBuffer.getSize());

  if (Settings::wireframe.get())
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

void ScenePass::OnResize(glm::uvec2 size)
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

wgpu::RenderPipeline ScenePass::CreatePipeline(wgpu::ShaderModule& shader,
                                               wgpu::PipelineLayout& pipelineLayout,
                                               const char* vertexEntryPoint,
                                               wgpu::PrimitiveTopology topology,
                                               const wgpu::VertexBufferLayout* vertexLayout)
{
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

  pipelineDesc.vertex.bufferCount  = vertexLayout ? 1 : 0;
  pipelineDesc.vertex.buffers      = vertexLayout;
  pipelineDesc.vertex.module       = shader;
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants    = nullptr;
  utils::SetEntryPoint(pipelineDesc.vertex.entryPoint, vertexEntryPoint);

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

void ScenePass::InitializeShaderVariants()
{
  const wgpu::ShaderStage vsfs = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  const wgpu::ShaderStage fs   = wgpu::ShaderStage::Fragment;

  { //blinn-phong shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::BlinnPhong)];

    v.bindGroupLayout = utils::CreateBindGroupLayout(this->context.device, {
      utils::CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get())),
      utils::CreateBufferLayout(1, fs,   wgpu::BufferBindingType::Uniform, utils::aligned_size(this->lightsData)),
      utils::CreateBufferLayout(2, fs,   wgpu::BufferBindingType::Uniform, utils::aligned_size(glm::vec4())),
    });

    v.bindGroup = utils::CreateBindGroup(
      this->context.device,
      std::vector<wgpu::BindGroupEntry>{
        utils::CreateBinding(0, this->mvpBuffer),
        utils::CreateBinding(1, this->lightBuffer),
        utils::CreateBinding(2, this->colorBuffer),
      }, v.bindGroupLayout);

    this->BuildVariant(v, "blinn_phong.wgsl", embedded_shaders::blinn_phong);
  }

  { //flat shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::Flat)];

    v.bindGroupLayout = utils::CreateBindGroupLayout(this->context.device, {
      utils::CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get())),
      utils::CreateBufferLayout(1, fs,   wgpu::BufferBindingType::Uniform, utils::aligned_size(glm::vec4())),
    });

    v.bindGroup = utils::CreateBindGroup(
      this->context.device,
      std::vector<wgpu::BindGroupEntry>{
        utils::CreateBinding(0, this->mvpBuffer),
        utils::CreateBinding(1, this->colorBuffer),
      }, v.bindGroupLayout);

    this->BuildVariant(v, "flat.wgsl", embedded_shaders::flat);
  }

  { //parametric error shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::ParametricError)];

    v.bindGroupLayout = utils::CreateBindGroupLayout(this->context.device, {
      utils::CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform,         utils::aligned_size(Settings::mvp.get())),
      utils::CreateBufferLayout(1, fs,   wgpu::BufferBindingType::Uniform,         utils::aligned_size(this->viewportData)),
      utils::CreateBufferLayout(2, fs,   wgpu::BufferBindingType::ReadOnlyStorage, 0),
    });

    v.bindGroup = utils::CreateBindGroup(
      this->context.device,
      std::vector<wgpu::BindGroupEntry>{
        utils::CreateBinding(0, this->mvpBuffer),
        utils::CreateBinding(1, this->viewportBuffer),
        utils::CreateBinding(2, this->controlPointsBuffer),
      }, v.bindGroupLayout);

    this->BuildVariant(v, "parametric_error.wgsl", embedded_shaders::parametric_error);
  }

  { //triangle size shader
    auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::TriangleSize)];

    v.bindGroupLayout = utils::CreateBindGroupLayout(this->context.device, {
      utils::CreateBufferLayout(0, vsfs, wgpu::BufferBindingType::Uniform, utils::aligned_size(Settings::mvp.get())),
    });

    v.bindGroup = utils::CreateBindGroup(
      this->context.device,
      std::vector<wgpu::BindGroupEntry>{
        utils::CreateBinding(0, this->mvpBuffer),
      }, v.bindGroupLayout);

    this->BuildVariant(v, "triangle_size.wgsl", embedded_shaders::triangle_size);
  }
}

void ScenePass::BuildVariant(ShaderVariant& v, const std::string& name, const std::string& source)
{
  // add ipass direct code to shader so it can call ipass_vertex()
  const std::string fullSource = source + "\n" + ipass::VertexTessPass::GetSurfaceShaderSource();

  v.shader = utils::LoadShaderFromSource(this->context.device, fullSource);
  if (!v.shader) throw std::runtime_error("Failed to load " + name);

  v.layout = this->CreatePipelineLayout({ v.bindGroupLayout });

  static_assert(ipass::SURFACE_BIND_GROUP == 1, "directLayout places the surface layout at index 1");
  v.directLayout = this->CreatePipelineLayout({ v.bindGroupLayout, this->vertexTessPass.GetSurfaceBindGroupLayout() });

  // pos(xyzw) normal(xyzw) uv(uv) patch_index, bary_id - read by vs_main
  static const std::array<wgpu::VertexAttribute, 5> attrs {
    utils::CreateAttribute(0, wgpu::VertexFormat::Float32x4, 0),
    utils::CreateAttribute(1, wgpu::VertexFormat::Float32x4, 4 * sizeof(glm::f32)),
    utils::CreateAttribute(2, wgpu::VertexFormat::Float32x2, 8 * sizeof(glm::f32)),
    utils::CreateAttribute(3, wgpu::VertexFormat::Float32,   10 * sizeof(glm::f32)),
    utils::CreateAttribute(4, wgpu::VertexFormat::Float32,   11 * sizeof(glm::f32)),
  };

  static const wgpu::VertexBufferLayout vertexLayoutData = [] {
    wgpu::VertexBufferLayout l;
    l.attributeCount = attrs.size();
    l.attributes     = attrs.data();
    l.arrayStride    = 12 * sizeof(glm::f32);
    l.stepMode       = wgpu::VertexStepMode::Vertex;
    return l;
  }();
  
  const wgpu::VertexBufferLayout* vertexLayout = &vertexLayoutData;
  
  v.pipeline          = this->CreatePipeline(v.shader, v.layout,       "vs_main",   wgpu::PrimitiveTopology::TriangleList, vertexLayout);
  v.wireframePipeline = this->CreatePipeline(v.shader, v.layout,       "vs_main",   wgpu::PrimitiveTopology::LineList,     vertexLayout);
  v.directPipeline    = this->CreatePipeline(v.shader, v.directLayout, "vs_direct", wgpu::PrimitiveTopology::TriangleList, nullptr);
}

wgpu::PipelineLayout ScenePass::CreatePipelineLayout(std::initializer_list<wgpu::BindGroupLayout> bindGroupLayouts)
{
  wgpu::PipelineLayoutDescriptor layoutDesc;
  layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
  layoutDesc.bindGroupLayouts = (const WGPUBindGroupLayout*)bindGroupLayouts.begin();
  return this->context.device.createPipelineLayout(layoutDesc);
}

wgpu::BlendState ScenePass::GetBlendState()
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

void ScenePass::CreateDepthTexture(glm::uvec2 size)
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

void ScenePass::RebuildParametricErrorBindGroup()
{
  auto& v = this->shaderVariants[static_cast<size_t>(ShadingMode::ParametricError)];
  if (v.bindGroup) v.bindGroup.release();
  v.bindGroup = utils::CreateBindGroup(
    this->context.device,
    std::vector<wgpu::BindGroupEntry>{
      utils::CreateBinding(0, this->mvpBuffer),
      utils::CreateBinding(1, this->viewportBuffer),
      utils::CreateBinding(2, this->controlPointsBuffer),
    }, v.bindGroupLayout);
}
