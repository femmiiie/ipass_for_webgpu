#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"
#include "LODPass.h"
#include "TessellationPass.h"

namespace ipass {

class Pipeline {
private:
    struct Impl;
    Impl* impl = nullptr;

public:
    // C compatible constructors 
    Pipeline(WGPUDevice device, const Config& config = {})
        : Pipeline(wgpu::Device(device), config) {}
    Pipeline(WGPUDevice device, WGPUQueue queue, const Config& config = {})
        : Pipeline(wgpu::Device(device), wgpu::Queue(queue), config) {}

    // C++ compatible construtors 
    Pipeline(wgpu::Device device, const Config& config = {})
        : Pipeline(device, device.getQueue(), config) {}
    Pipeline(wgpu::Device device, wgpu::Queue queue, const Config& config = {});

    ~Pipeline();

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&)                   noexcept;
    Pipeline& operator=(Pipeline&&)        noexcept;

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

    LODPass&          GetLOD();
    TessellationPass& GetTessellationPass();
};

}
