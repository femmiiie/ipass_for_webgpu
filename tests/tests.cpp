// Verification tests for the tessellator's GPU compute passes.
// Each TEST_CASE creates its own WebGPU device so tests stay isolated and can be run individually.

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "GPU.h"
#include "TessScanPass.h"
#include "Tessellator.h"

namespace {

void LogCallback(WGPULogLevel level, WGPUStringView msg, void*) {
    const char* level_str = (level == WGPULogLevel_Error) ? "ERROR" : (level == WGPULogLevel_Warn) ? "WARN" : "INFO";
    fprintf(stderr, "[wgpu %s] %.*s\n", level_str, (int)msg.length, msg.data);
}

void ErrorCallback(WGPUDevice const*, WGPUErrorType type, WGPUStringView msg, void*, void*) {
    fprintf(stderr, "[wgpu device error %d] %.*s\n", (int)type, (int)msg.length, msg.data);
}

struct TestGPU {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;

    TestGPU()
    {
        wgpuSetLogCallback(LogCallback, nullptr);
        wgpuSetLogLevel(WGPULogLevel_Warn);

        WGPUInstanceExtras instanceExtras = {};
        instanceExtras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
        instanceExtras.flags = WGPUInstanceFlag_Debug | WGPUInstanceFlag_Validation;

        wgpu::InstanceDescriptor instanceDesc = {};
        instanceDesc.nextInChain = &instanceExtras.chain;
        instance = wgpu::createInstance(instanceDesc);;
        REQUIRE(instance);
        
        wgpu::RequestAdapterOptions adapterOpts = {};
        adapter = instance.requestAdapter(adapterOpts);
        REQUIRE(adapter);

        wgpu::DeviceDescriptor deviceDesc = {};
        deviceDesc.label = WGPU_STRING_VIEW_INIT;
        deviceDesc.defaultQueue.label = WGPU_STRING_VIEW_INIT;
        deviceDesc.uncapturedErrorCallbackInfo.callback = ErrorCallback;
        device = adapter.requestDevice(deviceDesc);
        REQUIRE(device);

        queue = device.getQueue();
    }

    ~TestGPU()
    {
        queue.release();
        device.release();
        adapter.release();
        instance.release();
    }
};

void MapBuffer(wgpu::Device device, wgpu::Buffer buffer, wgpu::MapMode mode, size_t size)
{
    bool done = false;
    WGPUBufferMapCallbackInfo cbInfo = {};
    cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    cbInfo.callback = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        *static_cast<bool*>(ud) = true;
    };
    cbInfo.userdata1 = &done;
    wgpuBufferMapAsync(buffer, static_cast<WGPUMapMode>(mode), 0, size, cbInfo);
    while (!done)
        device.poll(true, nullptr);
}

} // namespace

TEST_CASE("exclusive prefix scan", "[scan]") {
    TestGPU gpu = TestGPU();

    const uint32_t N = 65536;
    static_assert(N <= 256 * 256, "N exceeds lvl2 capacity (max 65536)");

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 10);

    std::vector<uint32_t> h_in(N);
    for (auto& v : h_in)
        v = dist(rng);

    std::vector<uint32_t> h_ref(N);
    uint32_t acc = 0;
    for (uint32_t i = 0; i < N; i++) {
        h_ref[i] = acc;
        acc += h_in[i];
    }

    wgpu::Buffer buf_count = utils::CreateBuffer(gpu.device, N * 4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    wgpu::Buffer buf_offset = utils::CreateBuffer(gpu.device, N * 4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    wgpu::Buffer buf_blocks = utils::CreateBuffer(gpu.device, 256 * 4, wgpu::BufferUsage::Storage);
    wgpu::Buffer buf_total = utils::CreateBuffer(gpu.device, 4, wgpu::BufferUsage::Storage);
    wgpu::Buffer buf_staging = utils::CreateBuffer(gpu.device, N * 4, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);

    gpu.queue.writeBuffer(buf_count, 0, h_in.data(), N * 4);

    TessScanPass scan;
    REQUIRE(scan.Init(gpu.device));

    wgpu::BindGroupLayout lvl1_bgl = scan.GetLevel1BGL();
    wgpu::BindGroup bg1 = utils::CreateBindGroup(gpu.device, {
        utils::CreateBinding(0, buf_count),
        utils::CreateBinding(1, buf_offset),
        utils::CreateBinding(2, buf_blocks),
    }, lvl1_bgl);

    wgpu::BindGroupLayout lvl2_bgl = scan.GetLevel2BGL();
    wgpu::BindGroup bg2 = utils::CreateBindGroup(gpu.device, {
        utils::CreateBinding(0, buf_blocks),
        utils::CreateBinding(1, buf_total),
    }, lvl2_bgl);

    wgpu::BindGroupLayout comb_bgl = scan.GetCombineBGL();
    wgpu::BindGroup bgc = utils::CreateBindGroup(gpu.device, {
        utils::CreateBinding(1, buf_offset),
        utils::CreateBinding(2, buf_blocks),
    }, comb_bgl);

    scan.SetBindGroups(bg1, bg2, bgc);

    wgpu::CommandEncoderDescriptor encoderDesc = {};
    wgpu::CommandEncoder encoder = gpu.device.createCommandEncoder(encoderDesc);

    scan.Execute(encoder, N);
    encoder.copyBufferToBuffer(buf_offset, 0, buf_staging, 0, N * 4);

    wgpu::CommandBufferDescriptor commandDesc = {};
    wgpu::CommandBuffer commandBuffer = encoder.finish(commandDesc);
    encoder.release();
    gpu.queue.submit(1, &commandBuffer);
    commandBuffer.release();

    MapBuffer(gpu.device, buf_staging, wgpu::MapMode::Read, N * 4);

    const auto* result = static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buf_staging, 0, N * 4));

    uint32_t mismatches = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (result[i] != h_ref[i])
            mismatches++;
    }

    if (mismatches > 0) {
        for (uint32_t i = 0; i < N; i++) {
            if (result[i] != h_ref[i])
                UNSCOPED_INFO("index " << i << ": expected " << h_ref[i] << ", got " << result[i]);
        }
    }
    CHECK(mismatches == 0);

    wgpuBufferUnmap(buf_staging);

    buf_staging.release();
    buf_total.release();
    buf_blocks.release();
    buf_offset.release();
    buf_count.release();
    bgc.release();
    bg2.release();
    bg1.release();
}

TEST_CASE("tessellate single quad", "[tessellator]") {
    TestGPU gpu = TestGPU();

    glm::vec4 control_points[16];
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            control_points[row * 4 + col] = glm::vec4(row / 3.0f, col / 3.0f, 0.0f, 1.0f);

    uint32_t indices[4] = {0, 1, 2, 3};

    const uint32_t num_quads = 1;
    const float tess_level = 4.0f;

    wgpu::Buffer ipass_levels = utils::CreateBuffer(gpu.device, num_quads * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    gpu.queue.writeBuffer(ipass_levels, 0, &tess_level, sizeof(float));

    Tessellator tess(gpu.device, gpu.queue, num_quads);
    REQUIRE(tess.Init(ipass_levels));

    tess.Upload(control_points, indices, num_quads);

    wgpu::CommandEncoderDescriptor ed = {};
    wgpu::CommandEncoder encoder = gpu.device.createCommandEncoder(ed);
    REQUIRE(tess.Execute(encoder));

    const uint32_t num_tris = 32;
    const uint64_t readback_size = (uint64_t)num_tris * 3 * sizeof(glm::vec4);

    wgpu::Buffer staging = utils::CreateBuffer(gpu.device, readback_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);
    encoder.copyBufferToBuffer(tess.GetVertexOutput(), 0, staging, 0, readback_size);

    wgpu::CommandBufferDescriptor cbd = {};
    wgpu::CommandBuffer cmd = encoder.finish(cbd);
    encoder.release();
    gpu.queue.submit(1, &cmd);
    cmd.release();

    MapBuffer(gpu.device, staging, wgpu::MapMode::Read, readback_size);

    const auto* verts = static_cast<const glm::vec4*>(wgpuBufferGetConstMappedRange(staging, 0, readback_size));

    std::cout << "Single flat unit quad tessellated at TESS_LEVEL=" << tess_level << " -> " << num_tris << " triangles:\n\n";
    for (uint32_t t = 0; t < num_tris; t++) {
        const glm::vec4& v0 = verts[t * 3 + 0];
        const glm::vec4& v1 = verts[t * 3 + 1];
        const glm::vec4& v2 = verts[t * 3 + 2];
        std::cout << "tri[" << t << "]: " << "(" << v0.x << ", " << v0.y << ", " << v0.z << ")  " << "(" << v1.x << ", " << v1.y << ", " << v1.z << ")  " << "(" << v2.x << ", " << v2.y << ", " << v2.z << ")\n";
    }

    wgpuBufferUnmap(staging);

    tess.Terminate();
    staging.release();
    ipass_levels.release();
}
