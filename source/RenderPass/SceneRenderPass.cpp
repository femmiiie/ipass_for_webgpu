#include "SceneRenderPass.h"

SceneRenderPass::SceneRenderPass(RenderContext& ctx) : RenderPass(ctx)
{
  std::vector<glm::f32> data {//basic test triangle
    // position          normal               color                tex
    0.0, 0.0, 0.0, 1.0,  0.0, 0.0, 1.0, 0.0,  1.0, 0.0, 0.0, 1.0,  0.0, 0.0,
    0.5, 0.5, 0.0, 1.0,  0.0, 0.0, 1.0, 0.0,  0.0, 1.0, 0.0, 1.0,  0.0, 0.0,
    1.0, 0.0, 0.0, 1.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 1.0, 1.0,  0.0, 0.0,
  };
  this->vertexCount = (glm::u32)data.size() / 14;

  this->vertexBuffer = this->CreateBuffer(
    data.size() * sizeof(glm::f32),
    wgpu::BufferUsage(static_cast<WGPUBufferUsage>(wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex)),
    false
  );

  this->context.queue.writeBuffer(this->vertexBuffer, 0, data.data(), data.size() * sizeof(glm::f32));

  this->mvp = {
    .M = glm::mat4(1),
    .V = glm::lookAt(
            glm::vec3(0, 0, -2),
            glm::vec3(0),
            glm::vec3(0, 1, 0)
            ),
    .P = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.0f)
  };

  this->light = {
    .position = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
    .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .power = 10.0f
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
}


void SceneRenderPass::Execute(wgpu::RenderPassEncoder& encoder)
{
  encoder.setPipeline(this->pipeline);
  encoder.setVertexBuffer(0, this->vertexBuffer, 0, this->vertexBuffer.getSize());
  encoder.setBindGroup(0, this->bindGroup, 0, nullptr);
  encoder.draw(this->vertexCount, 1, 0, 0);
}

void SceneRenderPass::InitializeRenderPipeline()
{
  wgpu::ShaderModule shaderModule = context.LoadShader("shader.wgsl");
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

  pipelineDesc.depthStencil = nullptr;
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