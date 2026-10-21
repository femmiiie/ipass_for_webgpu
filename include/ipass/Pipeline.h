#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"
#include "LODPass.h"
#include "TessellationPass.h"

#include <string>

namespace ipass {

class ComputePipeline {
private:
    struct Impl;
    Impl* impl = nullptr;

public:
    // max_patches=0 autosizes based on device limits

    // C compatible constructors
    ComputePipeline(WGPUDevice device, uint32_t max_patches = 0)
        : ComputePipeline(wgpu::Device(device), max_patches) {}
    ComputePipeline(WGPUDevice device, WGPUQueue queue, uint32_t max_patches = 0)
        : ComputePipeline(wgpu::Device(device), wgpu::Queue(queue), max_patches) {}

    // C++ compatible construtors
    ComputePipeline(wgpu::Device device, uint32_t max_patches = 0)
        : ComputePipeline(device, device.getQueue(), max_patches) {}
    ComputePipeline(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches = 0);

    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&)            = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(ComputePipeline&&)                   noexcept;
    ComputePipeline& operator=(ComputePipeline&&)        noexcept;

    Status LoadPatches(const PatchData& data);

    void SetMVP(const glm::mat4& mvp);
    void SetViewport(float width, float height);

    Status DispatchLOD(WGPUCommandEncoder& encoder) { return DispatchLOD(wgpu::CommandEncoder(encoder)); }
    Status DispatchLOD(wgpu::CommandEncoder& encoder);

    Status DispatchTessellation(WGPUCommandEncoder& encoder) { return DispatchTessellation(wgpu::CommandEncoder(encoder)); }
    Status DispatchTessellation(wgpu::CommandEncoder& encoder);

    Status Execute(WGPUCommandEncoder& encoder) { return Execute(wgpu::CommandEncoder(encoder)); }
    Status Execute(wgpu::CommandEncoder& encoder);

    wgpu::Buffer GetVertexBuffer() const;
    uint32_t     GetMaxVertexCount() const;

    wgpu::Buffer GetLODBuffer() const;
    wgpu::Buffer GetControlPointBuffer() const;
    wgpu::Buffer GetTriCountBuffer() const; // total triangle count, written by Execute
    uint32_t     GetPatchCount() const;

    LODPass&                  GetLOD();
    ComputeTessPass& GetTessellationPass();
};

class DirectPipeline {
private:
    struct Impl;
    Impl* impl = nullptr;

public:
    // max_patches=0 autosizes based on device limits

    // C compatible constructors
    DirectPipeline(WGPUDevice device, uint32_t max_patches = 0)
        : DirectPipeline(wgpu::Device(device), max_patches) {}
    DirectPipeline(WGPUDevice device, WGPUQueue queue, uint32_t max_patches = 0)
        : DirectPipeline(wgpu::Device(device), wgpu::Queue(queue), max_patches) {}

    // C++ compatible construtors
    DirectPipeline(wgpu::Device device, uint32_t max_patches = 0)
        : DirectPipeline(device, device.getQueue(), max_patches) {}
    DirectPipeline(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches = 0);

    ~DirectPipeline();

    DirectPipeline(const DirectPipeline&)            = delete;
    DirectPipeline& operator=(const DirectPipeline&) = delete;
    DirectPipeline(DirectPipeline&&)                   noexcept;
    DirectPipeline& operator=(DirectPipeline&&)        noexcept;

    Status LoadPatches(const PatchData& data);

    void SetMVP(const glm::mat4& mvp);
    void SetViewport(float width, float height);

    Status DispatchLOD(WGPUCommandEncoder& encoder) { return DispatchLOD(wgpu::CommandEncoder(encoder)); }
    Status DispatchLOD(wgpu::CommandEncoder& encoder);

    Status DispatchTessellation(WGPUCommandEncoder& encoder) { return DispatchTessellation(wgpu::CommandEncoder(encoder)); }
    Status DispatchTessellation(wgpu::CommandEncoder& encoder);

    Status Execute(WGPUCommandEncoder& encoder) { return Execute(wgpu::CommandEncoder(encoder)); }
    Status Execute(wgpu::CommandEncoder& encoder);

    Status Draw(WGPURenderPassEncoder pass) { wgpu::RenderPassEncoder p(pass); return Draw(p); }
    Status Draw(wgpu::RenderPassEncoder& pass);

    wgpu::BindGroupLayout     GetSurfaceBindGroupLayout() const;
    static const std::string& GetSurfaceShaderSource() { return VertexTessPass::GetSurfaceShaderSource(); }

    wgpu::Buffer GetLODBuffer() const;
    wgpu::Buffer GetControlPointBuffer() const;
    wgpu::Buffer GetTriCountBuffer() const; // total triangle count, written by Execute
    uint32_t     GetPatchCount() const;

    LODPass&        GetLOD();
    VertexTessPass& GetTessellationPass();
};

}
