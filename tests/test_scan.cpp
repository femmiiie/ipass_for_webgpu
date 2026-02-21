// Verification tess-scan.h
// Generates random input, computes a CPU-side exclusive prefix sum, runs the three GPU passes (lvl1 / lvl2 / comb), reads back the result, and compares element-by-element.

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "tess-scan.h"

using namespace wgpu;

int main() {

    InstanceDescriptor instDesc = {};
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
    Device device = adapter.requestDevice(devDesc);
    if (!device) {
        std::cerr << "Failed to get device\n";
        return 1;
    }

    Queue queue = device.getQueue();

    const uint32_t N = 65536;
    static_assert(N <= 256 * 256, "N exceeds lvl2 capcity (max 65536)");

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

    auto make_buf = [&](const char* label, size_t llen, BufferUsage usage, uint64_t size) -> Buffer {
        BufferDescriptor bd = {};
        bd.label = {label, llen};
        bd.usage = usage;
        bd.size = size;
        bd.mappedAtCreation = false;
        return device.createBuffer(bd);
    };

    Buffer buf_count = make_buf("triCount",  8, BufferUsage::Storage | BufferUsage::CopyDst, N * 4);
    Buffer buf_offset = make_buf("triOffset", 9, BufferUsage::Storage | BufferUsage::CopySrc, N * 4);
    Buffer buf_blocks = make_buf("blockSums", 9, BufferUsage::Storage, 256 * 4);
    Buffer buf_total = make_buf("bs_total",  8, BufferUsage::Storage, 4);
    Buffer buf_staging = make_buf("staging", 7, BufferUsage::MapRead | BufferUsage::CopyDst, N * 4);

    queue.writeBuffer(buf_count, 0, h_in.data(), N * 4);

    TessScanPass scan;
    scan.init(device);

    auto bge = [](uint32_t binding, Buffer buf, uint64_t size) -> BindGroupEntry {
        BindGroupEntry e = {};
        e.binding = binding;
        e.buffer  = buf;
        e.offset  = 0;
        e.size    = size;
        return e;
    };

    // lvl1 BGL: triCount(0), triOffset(1), blockSums(2)
    std::array<BindGroupEntry, 3> e1 = {
        bge(0, buf_count,  N * 4),
        bge(1, buf_offset, N * 4),
        bge(2, buf_blocks, 256 * 4),
    };
    BindGroupDescriptor bgd = {};
    bgd.layout = scan.get_lvl1_bgl();
    bgd.entryCount = 3; bgd.entries = e1.data();
    BindGroup bg1 = device.createBindGroup(bgd);

    // lvl2 BGL: blockSums(0), bs_total(1)
    std::array<BindGroupEntry, 2> e2 = {
        bge(0, buf_blocks, 256 * 4),
        bge(1, buf_total,  4),
    };
    bgd.layout = scan.get_lvl2_bgl();
    bgd.entryCount = 2; bgd.entries = e2.data();
    BindGroup bg2 = device.createBindGroup(bgd);

    // comb BGL: triOffset(1), blockSums(2)
    std::array<BindGroupEntry, 2> ec = {
        bge(1, buf_offset, N * 4),
        bge(2, buf_blocks, 256 * 4),
    };
    bgd.layout = scan.get_comb_bgl();
    bgd.entryCount = 2; bgd.entries = ec.data();
    BindGroup bgc = device.createBindGroup(bgd);

    scan.set_bindgroups(bg1, bg2, bgc);

    CommandEncoderDescriptor ed = {};
    CommandEncoder encoder = device.createCommandEncoder(ed);

    scan.exec(encoder, N);
    encoder.copyBufferToBuffer(buf_offset, 0, buf_staging, 0, N * 4);

    CommandBufferDescriptor cbd = {};
    CommandBuffer cmd = encoder.finish(cbd);
    encoder.release();
    queue.submit(1, &cmd);
    cmd.release();

    bool map_done = false;
    WGPUBufferMapCallbackInfo cbInfo = {};
    cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    cbInfo.callback = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        *static_cast<bool*>(ud) = true;
    };
    cbInfo.userdata1 = &map_done;
    wgpuBufferMapAsync(buf_staging, WGPUMapMode_Read, 0, N * 4, cbInfo);
    while (!map_done)
        device.poll(true, nullptr);

    const auto* result = static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buf_staging, 0, N * 4));

    std::cout << "input: [";
    for (uint32_t i = 0; i < N; i++)
        std::cout << (i ? ", " : "") << h_in[i];
    std::cout << "]\n\n";

    std::cout << "expected: [";
    for (uint32_t i = 0; i < N; i++)
        std::cout << (i ? ", " : "") << h_ref[i];
    std::cout << "]\n\n";

    std::cout << "output: [";
    for (uint32_t i = 0; i < N; i++)
        std::cout << (i ? ", " : "") << result[i];
    std::cout << "]\n\n";

    bool ok = true;
    uint32_t mismatches = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (result[i] != h_ref[i]) {
            ok = false;
            mismatches++;
        }
    }

    wgpuBufferUnmap(buf_staging);

    std::cout << (ok ? "PASS" : "FAIL") << ": exclusive prefix scan, N=" << N << (ok ? "" : " (" + std::to_string(mismatches) + " mismatches)") << "\n";

    buf_staging.release();
    buf_total.release();
    buf_blocks.release();
    buf_offset.release();
    buf_count.release();
    bgc.release();
    bg2.release();
    bg1.release();
    queue.release();
    device.release();
    adapter.release();
    instance.release();

    return !ok;
}
