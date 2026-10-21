#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"

struct IPass;

namespace ipass {

class LODPass {
private:
    IPass* impl = nullptr;

public:
    // max_patches=0 autosizes based on device limits

    // C compatible constructors
    LODPass(WGPUDevice device, uint32_t max_patches = 0)
        : LODPass(wgpu::Device(device), max_patches) {}
    LODPass(WGPUDevice device, WGPUQueue queue, uint32_t max_patches = 0)
        : LODPass(wgpu::Device(device), wgpu::Queue(queue), max_patches) {}

    // C++ compatible construtors
    LODPass(wgpu::Device device, uint32_t max_patches = 0)
        : LODPass(device, device.getQueue(), max_patches) {}
    LODPass(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches = 0);

    ~LODPass();

    LODPass(const LODPass&)            = delete;
    LODPass& operator=(const LODPass&) = delete;
    LODPass(LODPass&&)                 = delete;
    LODPass& operator=(LODPass&&)      = delete;

    Status UploadPatches(const PatchData& data);

    void SetMVP(const glm::mat4& mvp);
    void SetViewport(float width, float height);

    Status Dispatch(WGPUCommandEncoder& encoder) { return Dispatch(wgpu::CommandEncoder(encoder)); }
    Status Dispatch(wgpu::CommandEncoder& encoder);

    wgpu::Buffer GetLODBuffer()  const;
    uint32_t     GetPatchCount() const;
};

}
