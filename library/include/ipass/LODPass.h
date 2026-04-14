#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"

namespace ipass {

class LODPass {
private:
    struct Impl; 
    Impl* impl = nullptr;

public:
    // C compatible constructors 
    LODPass(WGPUDevice device, const Config& config = {}) 
        : LODPass(wgpu::Device(device), config) {}
    LODPass(WGPUDevice device, WGPUQueue queue, const Config& config = {})
        : LODPass(wgpu::Device(device), wgpu::Queue(queue), config) {}

    // C++ compatible construtors 
    LODPass(wgpu::Device device, const Config& config = {})
        : LODPass(device, device.getQueue(), config) {}
    LODPass(wgpu::Device device, wgpu::Queue queue, const Config& config = {});

    ~LODPass();

    LODPass(const LODPass&)            = delete;
    LODPass& operator=(const LODPass&) = delete;
    LODPass(LODPass&&)                   noexcept;
    LODPass& operator=(LODPass&&)        noexcept;

    Status UploadPatches(const PatchData& data);

    void SetMVP(const glm::mat4& mvp);
    void SetViewport(float width, float height);

    Status Dispatch(WGPUCommandEncoder& encoder) { return Dispatch(wgpu::CommandEncoder(encoder)); }
    Status Dispatch(wgpu::CommandEncoder& encoder);

    wgpu::Buffer GetLODBuffer() const;
    uint32_t     GetPatchCount() const;
};

}
