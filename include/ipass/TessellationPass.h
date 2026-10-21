#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"

#include <string>

class ComputeTessellator;
class VertexTessellator;

namespace ipass {

// bindgroup index of the surface bindings
inline constexpr uint32_t SURFACE_BIND_GROUP = 1;

class ComputeTessPass {
private:
    ComputeTessellator* impl = nullptr;

public:
    // max_patches=0 autosizes based on device limits

    // C compatible constructors
    ComputeTessPass(WGPUDevice device, uint32_t max_patches = 0)
        : ComputeTessPass(wgpu::Device(device), max_patches) {}
    ComputeTessPass(WGPUDevice device, WGPUQueue queue, uint32_t max_patches = 0)
        : ComputeTessPass(wgpu::Device(device), wgpu::Queue(queue), max_patches) {}

    // C++ compatible construtors
    ComputeTessPass(wgpu::Device device, uint32_t max_patches = 0)
        : ComputeTessPass(device, device.getQueue(), max_patches) {}
    ComputeTessPass(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches = 0);

    ~ComputeTessPass();

    ComputeTessPass(const ComputeTessPass&)            = delete;
    ComputeTessPass& operator=(const ComputeTessPass&) = delete;
    ComputeTessPass(ComputeTessPass&&)                 = delete;
    ComputeTessPass& operator=(ComputeTessPass&&)      = delete;

    Status UploadPatches(const PatchData& data, wgpu::Buffer lod_buffer);

    Status Dispatch(WGPUCommandEncoder& encoder) { return Dispatch(wgpu::CommandEncoder(encoder)); }
    Status Dispatch(wgpu::CommandEncoder& encoder);

    wgpu::Buffer GetVertexBuffer() const;
    uint32_t     GetMaxVertexCount() const;

    wgpu::Buffer GetControlPointBuffer() const;
    wgpu::Buffer GetTriCountBuffer() const; // total triangle count
    uint32_t     GetPatchCount() const;
};

class VertexTessPass {
private:
    VertexTessellator* impl = nullptr;

public:
    // max_patches=0 autosizes based on device limits

    // C compatible constructors
    VertexTessPass(WGPUDevice device, uint32_t max_patches = 0)
        : VertexTessPass(wgpu::Device(device), max_patches) {}
    VertexTessPass(WGPUDevice device, WGPUQueue queue, uint32_t max_patches = 0)
        : VertexTessPass(wgpu::Device(device), wgpu::Queue(queue), max_patches) {}

    // C++ compatible construtors
    VertexTessPass(wgpu::Device device, uint32_t max_patches = 0)
        : VertexTessPass(device, device.getQueue(), max_patches) {}
    VertexTessPass(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches = 0);

    ~VertexTessPass();

    VertexTessPass(const VertexTessPass&)            = delete;
    VertexTessPass& operator=(const VertexTessPass&) = delete;
    VertexTessPass(VertexTessPass&&)                 = delete;
    VertexTessPass& operator=(VertexTessPass&&)      = delete;

    Status UploadPatches(const PatchData& data, wgpu::Buffer lod_buffer);

    Status Dispatch(WGPUCommandEncoder& encoder) { return Dispatch(wgpu::CommandEncoder(encoder)); }
    Status Dispatch(wgpu::CommandEncoder& encoder);

    // binds the surface bindgroup and issue an indirect draw
    Status Draw(WGPURenderPassEncoder pass) { wgpu::RenderPassEncoder p(pass); return Draw(p); }
    Status Draw(wgpu::RenderPassEncoder& pass);

    // available before patches are loaded, so render pipelines can be built up front
    wgpu::BindGroupLayout GetSurfaceBindGroupLayout() const;

    // WGSL code defining ipass_vertex(vertex_index, instance_index) with its bindings at SURFACE_BIND_GROUP
    static const std::string& GetSurfaceShaderSource();

    wgpu::Buffer GetControlPointBuffer() const;
    wgpu::Buffer GetTriCountBuffer() const; // total triangle count, written by Dispatch
    uint32_t     GetPatchCount() const;
};

}
