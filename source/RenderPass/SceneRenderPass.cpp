#include "SceneRenderPass.h"

#include <array>
#include <fstream>
#include <sstream>

void SceneRenderPass::LoadOBJ()
{
  const std::string filepath = "C:/Users/Sandro/Personal/Downloads/objects/horse.obj"; // hardcoded - change as needed

  std::ifstream file(filepath);
  if (!file.is_open())
  {
    std::cerr << "[LoadOBJ] Failed to open: " << filepath << std::endl;
    return;
  }

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> texcoords;

  std::vector<glm::f32> vertexData;

  std::string line;
  while (std::getline(file, line))
  {
    std::istringstream ss(line);
    std::string token;
    ss >> token;

    if (token == "v")
    {
      glm::vec3 p;
      ss >> p.x >> p.y >> p.z;
      positions.push_back(p);
    }
    else if (token == "vn")
    {
      glm::vec3 n;
      ss >> n.x >> n.y >> n.z;
      normals.push_back(n);
    }
    else if (token == "vt")
    {
      glm::vec2 uv;
      ss >> uv.x >> uv.y;
      texcoords.push_back(uv);
    }
    else if (token == "f")
    {
      // Collect face corners, then fan-triangulate
      std::vector<std::array<int,3>> corners; // [posIdx, uvIdx, normIdx] (0-based, -1 if absent)
      std::string corner;
      while (ss >> corner)
      {
        std::array<int, 3> idx {-1, -1, -1};
        std::istringstream cs(corner);
        std::string part;
        int slot = 0;
        while (std::getline(cs, part, '/') && slot < 3)
        {
          if (!part.empty()) idx[slot] = std::stoi(part) - 1;
          slot++;
        }
        corners.push_back(idx);
      }

      auto pushVertex = [&](const std::array<int,3>& c)
      {
        glm::vec3 pos = (c[0] >= 0 && c[0] < (int)positions.size()) ? positions[c[0]] : glm::vec3(0);
        glm::vec2 uv  = (c[1] >= 0 && c[1] < (int)texcoords.size()) ? texcoords[c[1]] : glm::vec2(0);
        glm::vec3 nrm = (c[2] >= 0 && c[2] < (int)normals.size())   ? normals[c[2]]   : glm::vec3(0,0,1);
        // pos(xyzw) normal(xyzw) color(rgba) uv(uv)
        vertexData.insert(vertexData.end(), { pos.x, pos.y, pos.z, 1.0f,
                                              nrm.x, nrm.y, nrm.z, 0.0f,
                                              1.0f, 1.0f, 1.0f, 1.0f,
                                              uv.x, uv.y });
      };

      // Fan triangulation: (0,1,2), (0,2,3), ...
      for (size_t i = 1; i + 1 < corners.size(); i++)
      {
        pushVertex(corners[0]);
        pushVertex(corners[i]);
        pushVertex(corners[i + 1]);
      }
    }
  }

  if (vertexData.empty())
  {
    std::cerr << "[LoadOBJ] No vertex data parsed from: " << filepath << std::endl;
    return;
  }

  this->vertexCount = (glm::u32)vertexData.size() / 14;

  if (this->vertexBuffer)
    this->vertexBuffer.destroy();

  this->vertexBuffer = this->CreateBuffer(
    vertexData.size() * sizeof(glm::f32),
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex)),
    false
  );

  this->context.queue.writeBuffer(this->vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(glm::f32));

  std::cout << "[LoadOBJ] Loaded " << this->vertexCount << " vertices from " << filepath << std::endl;
}

SceneRenderPass::SceneRenderPass(RenderContext& context) : RenderPass(context)
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

  // std::vector<glm::f32> data { // basic test triangle
  //   // position          normal               color                tex
  //   0.0, 0.0, 0.0, 1.0,  0.0, 0.0, 1.0, 0.0,  1.0, 0.0, 0.0, 1.0,  0.0, 0.0,
  //   0.5, 0.5, 0.0, 1.0,  0.0, 0.0, 1.0, 0.0,  0.0, 1.0, 0.0, 1.0,  0.0, 0.0,
  //   1.0, 0.0, 0.0, 1.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 1.0, 1.0,  0.0, 0.0,
  // };
  // this->vertexCount = (glm::u32)data.size() / 14;

  // this->vertexBuffer = this->CreateBuffer(
  //   data.size() * sizeof(glm::f32),
  //   wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex)),
  //   false
  // );

  // this->context.queue.writeBuffer(this->vertexBuffer, 0, data.data(), data.size() * sizeof(glm::f32));

  this->LoadOBJ();
  this->CreateDepthTexture(context.size);

  this->mvp = {
    .M = glm::mat4(1),
    .M_inv = glm::inverse(glm::mat4(1)),
    .V = glm::lookAt(
            glm::vec3(0, -2, 0),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0)
            ),
    .P = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.0f)
  };

  this->light = {
    .position = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
    .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .power = 5.0f
  };

  std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts {
    this->CreateBindingLayout(0, wgpu::ShaderStage(static_cast<WGPUShaderStage>(wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment)), utils::aligned_size(this->mvp)),
    this->CreateBindingLayout(1, wgpu::ShaderStage::Fragment, utils::aligned_size(this->light)),
  };
  this->bindGroupLayout = this->CreateBindGroupLayout(bindingLayouts);

  std::vector<wgpu::BindGroupEntry> bindings {
    this->CreateBinding(0, this->mvpBuffer, utils::aligned_size(this->mvp)),
    this->CreateBinding(1, this->lightBuffer, utils::aligned_size(this->light))
  };
  this->bindGroup = this->CreateBindGroup(bindings);

  this->context.queue.writeBuffer(this->mvpBuffer, 0, &this->mvp, sizeof(MVP));
  this->context.queue.writeBuffer(this->lightBuffer, 0, &this->light, sizeof(Light));

  this->InitializeRenderPipeline();
}

SceneRenderPass::~SceneRenderPass()
{
  this->pipeline.release();
  this->layout.release();
  this->bindGroupLayout.release();
  this->bindGroup.release();
  if (this->depthTextureView) this->depthTextureView.release();
  if (this->depthTexture)     this->depthTexture.destroy();
}


void SceneRenderPass::Execute(wgpu::RenderPassEncoder& encoder)
{
  bool mvpNeedsUpdate = false;

  const RenderContext::Viewport& vp = context.sceneViewport;
  encoder.setViewport(vp.x, vp.y, vp.width, vp.height, 0.0f, 1.0f);
  encoder.setScissorRect((uint32_t)vp.x, (uint32_t)vp.y,
                         (uint32_t)vp.width, (uint32_t)vp.height);

  if (Settings::resetTransformUpdate())
  {
    glm::mat4 T  = glm::translate(glm::mat4(1.0f), Settings::translation);
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(Settings::rotation.x), glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), glm::radians(Settings::rotation.y), glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), glm::radians(Settings::rotation.z), glm::vec3(0, 0, 1));
    glm::mat4 S  = glm::scale(glm::mat4(1.0f), Settings::scale);
    mvp.M     = T * Rz * Ry * Rx * S;
    mvp.M_inv = glm::inverse(mvp.M);
    mvpNeedsUpdate  = true;
  }

  if (camera.requiresUpdate())
  {
    camera.update();
    mvp.P = camera.getProjectionMatrix();
    mvpNeedsUpdate = true;
  }

  if (camera.consumeViewUpdate())
  {
    mvp.V = camera.getViewMatrix();
    mvpNeedsUpdate = true;
  }

  if (mvpNeedsUpdate)
  {
    context.queue.writeBuffer(mvpBuffer, 0, &mvp, sizeof(MVP));
  }

  encoder.setPipeline(this->pipeline);
  encoder.setVertexBuffer(0, this->vertexBuffer, 0, this->vertexBuffer.getSize());
  encoder.setBindGroup(0, this->bindGroup, 0, nullptr);
  encoder.draw(this->vertexCount, 1, 0, 0);
}

void SceneRenderPass::OnResize(glm::uvec2 size)
{
  const RenderContext::Viewport& vp = context.sceneViewport;
  camera.getAspect_M() = (vp.height > 0.0f) ? vp.width / vp.height : 1.0f;
  camera.deferUpdate();
  this->CreateDepthTexture(size);
}

void SceneRenderPass::InitializeRenderPipeline()
{
  wgpu::ShaderModule shaderModule = utils::LoadShader(this->context.device, "shader.wgsl");
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
  vertexBufferLayout.arrayStride = 14 * sizeof(glm::f32);
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
  pipelineDesc.primitive.cullMode = wgpu::CullMode::Back;

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