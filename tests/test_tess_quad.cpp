#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "Tessellator.h"

using namespace wgpu;

static void wgpu_log_callback(WGPULogLevel level, WGPUStringView msg, void*) {
    const char* ls;
    if (level == WGPULogLevel_Error) {
        ls = "ERROR";
    } else if (level == WGPULogLevel_Warn) {
        ls = "WARN";
    } else {
        ls = "INFO";
    }
    fprintf(stderr, "[wgpu %s] %.*s\n", ls, (int)msg.length, msg.data);
}

static void wgpu_error_callback(WGPUDevice const*, WGPUErrorType type, WGPUStringView msg, void*, void*) {
    fprintf(stderr, "[wgpu device error %d] %.*s\n", (int)type, (int)msg.length, msg.data);
}

static void wgpu_map_callback(WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
    *static_cast<bool*>(ud) = true;
}

int main() {
    wgpuSetLogCallback(wgpu_log_callback, nullptr);
    wgpuSetLogLevel(WGPULogLevel_Warn);

    WGPUInstanceExtras instExtras = {};
    instExtras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
    instExtras.backends = WGPUInstanceBackend_Vulkan;
    instExtras.flags = WGPUInstanceFlag_Debug | WGPUInstanceFlag_Validation;

    InstanceDescriptor instDesc = {};
    instDesc.nextInChain = &instExtras.chain;
    Instance instance = wgpuCreateInstance(&instDesc);
    if (!instance) {
        std::cerr << "Failed to create WebGPU instance\n";
        return 1;
    }

    RequestAdapterOptions adapterOpts = {};
    Adapter adapter = instance.requestAdapter(adapterOpts);
    if (!adapter) {
        std::cerr << "Failed to get adapter\n";
        return 1;
    }

    DeviceDescriptor devDesc = {};
    devDesc.label = WGPU_STRING_VIEW_INIT;
    devDesc.defaultQueue.label = WGPU_STRING_VIEW_INIT;
    devDesc.requiredFeatureCount = 0;
    devDesc.requiredLimits = nullptr;
    devDesc.uncapturedErrorCallbackInfo.callback = wgpu_error_callback;
    Device device = adapter.requestDevice(devDesc);
    if (!device) {
        std::cerr << "Failed to get device\n";
        return 1;
    }

    Queue queue = device.getQueue();

    glm::vec4 control_points[16];
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            control_points[row * 4 + col] = glm::vec4(row / 3.0f, col / 3.0f, 0.0f, 1.0f);

    uint32_t indices[4] = {0, 1, 2, 3};

    const uint32_t num_quads = 1;

    GPUContext ctx = {device, queue};
    Tessellator tess(ctx);
    if (!tess.Init(num_quads, wgpu::Buffer())) { // needs to be fixed to setup ipass buffer
        std::cerr << "Failed to init Tessellator\n";
        return 1;
    }

    tess.Upload(control_points, indices, num_quads);

    CommandEncoderDescriptor ed = {};
    CommandEncoder encoder = device.createCommandEncoder(ed);

    if (!tess.Execute(encoder, num_quads)) {
        std::cerr << "Failed to exec Tessellator\n";
        return 1;
    }

    const uint32_t num_tris = 32;
    const uint64_t readback_size = (uint64_t)num_tris * 3 * sizeof(glm::vec4);

    BufferDescriptor stagingDesc = {};
    stagingDesc.size = readback_size;
    stagingDesc.usage = BufferUsage::MapRead | BufferUsage::CopyDst;
    Buffer staging = device.createBuffer(stagingDesc);

    encoder.copyBufferToBuffer(tess.GetVertexOutput(), 0, staging, 0, readback_size);

    CommandBufferDescriptor cbd = {};
    CommandBuffer cmd = encoder.finish(cbd);
    encoder.release();
    queue.submit(1, &cmd);
    cmd.release();

    bool map_done = false;
    WGPUBufferMapCallbackInfo cbInfo = {};
    cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    cbInfo.callback = wgpu_map_callback;
    cbInfo.userdata1 = &map_done;
    wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, readback_size, cbInfo);
    while (!map_done)
        device.poll(true, nullptr);

    const auto* verts = static_cast<const glm::vec4*>(wgpuBufferGetConstMappedRange(staging, 0, readback_size));

    std::cout << "Single flat unit quad tessellated at TESS_LEVEL=4 -> " << num_tris << " triangles:\n\n";
    for (uint32_t t = 0; t < num_tris; t++) {
        const glm::vec4& v0 = verts[t * 3 + 0];
        const glm::vec4& v1 = verts[t * 3 + 1];
        const glm::vec4& v2 = verts[t * 3 + 2];
        std::cout << "tri[" << t << "]: " << "(" << v0.x << ", " << v0.y << ", " << v0.z << ")  " << "(" << v1.x << ", " << v1.y << ", " << v1.z << ")  " << "(" << v2.x << ", " << v2.y << ", " << v2.z << ")\n";
    }

    wgpuBufferUnmap(staging);

    tess.Terminate();
    staging.release();
    queue.release();
    device.release();
    adapter.release();
    instance.release();

    return 0;
}
