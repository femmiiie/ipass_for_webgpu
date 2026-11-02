// Verification tests for the tessellator's GPU passes.
// Each TEST_CASE creates its own WebGPU device so tests stay isolated and can be run individually.

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "GPU.h"
#include "Shader.h"
#include "TessScanPass.h"
#include "ComputeTessellator.h"
#include "VertexTessellator.h"
#include "TessConstants.h"

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
        instance = wgpu::createInstance(instanceDesc);
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

    wgpu::CommandEncoder Encoder()
    {
        wgpu::CommandEncoderDescriptor ed = {};
        return device.createCommandEncoder(ed);
    }

    void Submit(wgpu::CommandEncoder encoder)
    {
        wgpu::CommandBufferDescriptor cbd = {};
        wgpu::CommandBuffer cmd = encoder.finish(cbd);
        encoder.release();
        queue.submit(1, &cmd);
        cmd.release();
    }

    // copies the first `count` elements of a buffer back to the host, blocking until done
    template <typename T>
    std::vector<T> Readback(wgpu::Buffer src, uint64_t count)
    {
        const uint64_t size = count * sizeof(T);
        std::vector<T> out(count);
        if (size == 0)
            return out;

        wgpu::Buffer staging = utils::CreateBuffer(device, size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);
        wgpu::CommandEncoder encoder = Encoder();
        encoder.copyBufferToBuffer(src, 0, staging, 0, size);
        Submit(encoder);

        bool done = false;
        WGPUBufferMapCallbackInfo cbInfo = {};
        cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
        cbInfo.callback = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) { *static_cast<bool*>(ud) = true; };
        cbInfo.userdata1 = &done;
        wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, size, cbInfo);
        while (!done)
            device.poll(true, nullptr);

        std::memcpy(out.data(), wgpuBufferGetConstMappedRange(staging, 0, size), size);
        wgpuBufferUnmap(staging);
        staging.release();
        return out;
    }
};

// output vertex of both tessellators, IPassVertex in tess-eval.wgsl
struct TessVertex {
    glm::vec4 pos;
    glm::vec4 norm;
    glm::vec2 uv;
    float patch_idx;
    float bary_id;
};
static_assert(sizeof(TessVertex) == 48);

bool Near(const TessVertex& a, const TessVertex& b)
{
    auto near = [](auto x, auto y, float eps) { return glm::all(glm::lessThanEqual(glm::abs(x - y), decltype(x)(eps))); };
    return near(a.pos, b.pos, 1e-5f) && near(a.norm, b.norm, 1e-4f) && near(a.uv, b.uv, 1e-6f)
        && a.patch_idx == b.patch_idx && a.bary_id == b.bary_id;
}

using ByPatch = std::vector<std::vector<TessVertex>>;

// rational bicubic patches on a gx * gy grid, patch (px, py) spans x in [px, px + 1], y in [py, py + 1].
// control points are sampled from global functions so neighbouring patches share their edge rows exactly
struct PatchGrid {
    std::vector<glm::vec4> control_points;
    std::vector<uint32_t> indices;
    uint32_t gx, gy, num_patches;

    PatchGrid(uint32_t gx, uint32_t gy) : gx(gx), gy(gy), num_patches(gx * gy)
    {
        for (uint32_t py = 0; py < gy; py++) {
            for (uint32_t px = 0; px < gx; px++) {
                for (uint32_t row = 0; row < 4; row++) {     // u, along x
                    for (uint32_t col = 0; col < 4; col++) { // v, along y
                        float x = px + row / 3.0f;
                        float y = py + col / 3.0f;
                        float z = 0.3f * std::sin(x) * std::cos(y);
                        float w = 1.0f + 0.2f * std::sin(3.0f * x + y);
                        control_points.push_back(glm::vec4(x * w, y * w, z * w, w));
                    }
                }
                // corners (u, v): (0,0) (1,0) (1,1) (0,1), edge i joins corner i and i + 1 like tess side i
                auto vid = [&](uint32_t x, uint32_t y) { return y * (gx + 1) + x; };
                indices.insert(indices.end(), {vid(px, py), vid(px + 1, py), vid(px + 1, py + 1), vid(px, py + 1)});
            }
        }
    }
};

// the same patches and levels through both tessellators
struct TessRun {
    TestGPU& gpu;
    wgpu::Buffer levels;
    ComputeTessellator geometry;
    VertexTessellator direct;

    TessRun(TestGPU& gpu, const PatchGrid& grid, const std::vector<float>& tess_levels)
        : gpu(gpu), geometry(gpu.device, gpu.queue, grid.num_patches), direct(gpu.device, gpu.queue, grid.num_patches)
    {
        levels = utils::CreateBuffer(gpu.device, grid.num_patches * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
        gpu.queue.writeBuffer(levels, 0, tess_levels.data(), grid.num_patches * sizeof(float));

        REQUIRE(geometry.Init(levels));
        REQUIRE(direct.Init(levels));
        geometry.Upload(grid.control_points.data(), grid.indices.data(), grid.num_patches);
        direct.Upload(grid.control_points.data(), grid.indices.data(), grid.num_patches);

        wgpu::CommandEncoder encoder = gpu.Encoder();
        REQUIRE(geometry.Execute(encoder));
        REQUIRE(direct.Execute(encoder));
        gpu.Submit(encoder);
    }

    ~TessRun()
    {
        geometry.Terminate();
        direct.Terminate();
        levels.release();
    }

    std::vector<uint32_t> DrawArgs() { return gpu.Readback<uint32_t>(direct.GetIndirectBuffer(), 4); }
    uint32_t GeometryTris() { return gpu.Readback<uint32_t>(geometry.GetTriCountBuffer(), 1)[0]; }
    uint32_t DirectTris() { return gpu.Readback<uint32_t>(direct.GetTriCountBuffer(), 1)[0]; }

    // geometry output, triangles grouped by patch in emission order
    ByPatch GeometryByPatch()
    {
        ByPatch out(geometry.GetNumQuads());
        for (const TessVertex& v : gpu.Readback<TessVertex>(geometry.GetVertexOutput(), (uint64_t)GeometryTris() * 3))
            out[(uint32_t)v.patch_idx].push_back(v);
        return out;
    }

    // runs ipass_vertex for every vertex of the indirect draw from a compute shader, as Draw would in a vertex shader
    ByPatch DirectByPatch()
    {
        auto args = DrawArgs();
        REQUIRE(args[0] == patch::CHUNK_TRIS * 3);
        const uint32_t count = args[0] * args[1];
        ByPatch out(geometry.GetNumQuads());
        if (count == 0)
            return out;

        const std::string source = VertexTessellator::SurfaceShaderSource(0) + R"(
            @group(1) @binding(0) var<storage, read_write> harness_out : array<IPassVertex>;

            @compute @workgroup_size(64)
            fn harness(@builtin(global_invocation_id) gid : vec3u) {
                if (gid.x < arrayLength(&harness_out)) {
                    harness_out[gid.x] = ipass_vertex(gid.x % (IPASS_CHUNK_TRIS * 3u), gid.x / (IPASS_CHUNK_TRIS * 3u));
                }
            }
        )";
        wgpu::ShaderModule module = utils::LoadShaderFromSource(gpu.device, source);
        REQUIRE(module);

        wgpu::BindGroupLayout layouts[2] = {
            direct.GetSurfaceBindGroupLayout(),
            utils::CreateBindGroupLayout(gpu.device, {
                utils::CreateBufferLayout(0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage, 0),
            }),
        };
        wgpu::PipelineLayoutDescriptor pld = {};
        pld.bindGroupLayoutCount = 2;
        pld.bindGroupLayouts = (WGPUBindGroupLayout*)layouts;
        wgpu::PipelineLayout layout = gpu.device.createPipelineLayout(pld);

        wgpu::ComputePipelineDescriptor pd = {};
        pd.layout = layout;
        pd.compute.module = module;
        pd.compute.entryPoint = {"harness", 7};
        wgpu::ComputePipeline pipeline = gpu.device.createComputePipeline(pd);

        wgpu::Buffer out_buf = utils::CreateBuffer(gpu.device, (uint64_t)count * sizeof(TessVertex), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
        wgpu::BindGroup bg = utils::CreateBindGroup(gpu.device, {utils::CreateBinding(0, out_buf)}, layouts[1]);

        wgpu::CommandEncoder encoder = gpu.Encoder();
        wgpu::ComputePassDescriptor cpd = {};
        wgpu::ComputePassEncoder pass = encoder.beginComputePass(cpd);
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, direct.GetSurfaceBindGroup(), 0, nullptr);
        pass.setBindGroup(1, bg, 0, nullptr);
        pass.dispatchWorkgroups((count + 63) / 64, 1, 1);
        pass.end();
        pass.release();
        gpu.Submit(encoder);

        auto verts = gpu.Readback<TessVertex>(out_buf, count);
        for (uint32_t t = 0; t < count / 3; t++) {
            const TessVertex* tri = &verts[t * 3];
            // padding in a patch's last chunk has a zero normal and collapses to one point so the rasterizer drops it
            if (tri[0].norm == glm::vec4(0.0f)) {
                CHECK(tri[1].pos == tri[0].pos);
                CHECK(tri[2].pos == tri[0].pos);
                continue;
            }
            for (int c = 0; c < 3; c++)
                out[(uint32_t)tri[c].patch_idx].push_back(tri[c]);
        }

        bg.release();
        out_buf.release();
        pipeline.release();
        layout.release();
        layouts[1].release();
        module.release();
        return out;
    }
};

float UVArea(const TessVertex* tri)
{
    glm::vec2 ab = tri[1].uv - tri[0].uv, ac = tri[2].uv - tri[0].uv;
    return 0.5f * std::abs(ab.x * ac.y - ab.y * ac.x);
}

// every patch is covered exactly once in uv, and every vertex a patch places on a shared edge is also placed there by the neighbour
void CheckWatertight(const PatchGrid& grid, const ByPatch& by_patch)
{
    for (uint32_t p = 0; p < grid.num_patches; p++) {
        INFO("patch " << p);
        REQUIRE(!by_patch[p].empty());
        float area = 0.0f;
        uint32_t degenerate = 0;
        for (size_t t = 0; t < by_patch[p].size(); t += 3) {
            float a = UVArea(&by_patch[p][t]);
            area += a;
            if (a <= 0.0f)
                degenerate++;
        }
        CHECK(std::abs(area - 1.0f) < 1e-4f);
        CHECK(degenerate == 0);
    }

    auto edge_points = [&](uint32_t p, bool vertical, float line) {
        std::vector<glm::vec4> pts;
        for (const TessVertex& v : by_patch[p]) {
            if (std::abs((vertical ? v.pos.x : v.pos.y) - line) < 1e-6f)
                pts.push_back(v.pos);
        }
        std::sort(pts.begin(), pts.end(), [](const glm::vec4& a, const glm::vec4& b) {
            return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
        });
        pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
        return pts;
    };
    for (uint32_t py = 0; py < grid.gy; py++) {
        for (uint32_t px = 0; px < grid.gx; px++) {
            uint32_t p = py * grid.gx + px;
            if (px + 1 < grid.gx) {
                INFO("patches " << p << " and " << p + 1);
                CHECK(edge_points(p, true, px + 1.0f) == edge_points(p + 1, true, px + 1.0f));
            }
            if (py + 1 < grid.gy) {
                INFO("patches " << p << " and " << p + grid.gx);
                CHECK(edge_points(p, false, py + 1.0f) == edge_points(p + grid.gx, false, py + 1.0f));
            }
        }
    }
}

} // namespace

TEST_CASE("exclusive prefix scan", "[scan]") {
    TestGPU gpu;

    const uint32_t N = patch::MAX_SCAN;
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 10);
    std::vector<uint32_t> counts(N), expected(N);
    uint32_t total = 0;
    for (uint32_t i = 0; i < N; i++) {
        counts[i] = dist(rng);
        expected[i] = total;
        total += counts[i];
    }

    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer buf_counts = utils::CreateBuffer(gpu.device, N * sizeof(uint32_t), usage);
    wgpu::Buffer buf_offsets = utils::CreateBuffer(gpu.device, N * sizeof(uint32_t), usage);
    wgpu::Buffer buf_blocks = utils::CreateBuffer(gpu.device, 256 * sizeof(uint32_t), usage);
    wgpu::Buffer buf_total = utils::CreateBuffer(gpu.device, sizeof(uint32_t), usage);
    gpu.queue.writeBuffer(buf_counts, 0, counts.data(), N * sizeof(uint32_t));

    TessScanPass scan;
    REQUIRE(scan.Init(gpu.device));
    scan.Bind(buf_counts, buf_offsets, buf_blocks, buf_total);

    wgpu::CommandEncoder encoder = gpu.Encoder();
    scan.Execute(encoder, N);
    gpu.Submit(encoder);

    auto offsets = gpu.Readback<uint32_t>(buf_offsets, N);
    auto first_diff = std::mismatch(offsets.begin(), offsets.end(), expected.begin()).first;
    INFO("first mismatch at index " << (first_diff - offsets.begin()));
    CHECK(first_diff == offsets.end());
    CHECK(gpu.Readback<uint32_t>(buf_total, 1)[0] == total);

    buf_total.release();
    buf_blocks.release();
    buf_offsets.release();
    buf_counts.release();
}

TEST_CASE("direct draw matches geometry output", "[direct]") {
    TestGPU gpu;

    PatchGrid grid(6, 5);
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> dist(1.5f, 64.0f);
    std::vector<float> levels(grid.num_patches);
    for (auto& l : levels)
        l = dist(rng);
    levels[3] = 0.0f;   // culled
    levels[4] = 64.0f;  // MAX_TRIS
    levels[11] = 2.0f;  // empty inner grid

    TessRun run(gpu, grid, levels);
    auto geometry = run.GeometryByPatch();
    auto direct = run.DirectByPatch();

    uint32_t chunks = 0;
    for (uint32_t p = 0; p < grid.num_patches; p++) {
        INFO("patch " << p << " level " << levels[p]);
        REQUIRE(direct[p].size() == geometry[p].size());
        chunks += (uint32_t)((direct[p].size() / 3 + patch::CHUNK_TRIS - 1) / patch::CHUNK_TRIS);

        uint32_t mismatches = 0;
        for (size_t v = 0; v < direct[p].size(); v++) {
            if (!Near(direct[p][v], geometry[p][v]))
                mismatches++;
        }
        CHECK(mismatches == 0);
    }
    CHECK(direct[3].empty());
    CHECK(direct[4].size() == patch::MAX_TRIS * 3);
    CHECK(run.DirectTris() == run.GeometryTris());

    auto args = run.DrawArgs();
    CHECK(args[1] == chunks);
    CHECK(args[2] == 0);
    CHECK(args[3] == 0);
}

TEST_CASE("tessellation covers each patch without cracks", "[tessellator]") {
    TestGPU gpu;

    PatchGrid grid(3, 3);
    std::vector<float> levels = {3.2f, 17.0f, 1.0f,
                                 64.0f, 5.5f, 2.0f,
                                 9.0f, 1.0f, 40.0f};

    TessRun run(gpu, grid, levels);
    {
        INFO("geometry");
        CheckWatertight(grid, run.GeometryByPatch());
    }
    {
        INFO("direct");
        CheckWatertight(grid, run.DirectByPatch());
    }
}

TEST_CASE("direct draw of a level 1 patch is a single quad", "[direct]") {
    TestGPU gpu;

    PatchGrid grid(1, 1);
    TessRun run(gpu, grid, {1.0f});

    CHECK(run.DrawArgs()[1] == 1);
    CHECK(run.DirectTris() == 2);

    auto direct = run.DirectByPatch();
    REQUIRE(direct[0].size() == 6);
    const glm::vec2 expected[6] = {{0, 0}, {0, 1}, {1, 1}, {1, 1}, {1, 0}, {0, 0}};
    for (int v = 0; v < 6; v++) {
        CHECK(direct[0][v].uv == expected[v]);
        CHECK(direct[0][v].bary_id == float(v % 3));
    }
}

TEST_CASE("patch limits per output mode", "[limits]") {
    TestGPU gpu;

    CHECK(patch::ResolveMax(gpu.device, 100, patch::GEOMETRY_BYTES) == 100);
    CHECK(patch::ResolveMax(gpu.device, patch::MAX_SCAN + 1, patch::GEOMETRY_BYTES) == patch::MAX_SCAN);
    // derived from device limits, the direct path holds far more patches than the geometry vertex buffer allows
    CHECK(patch::ResolveMax(gpu.device, 0, patch::DIRECT_BYTES) > patch::ResolveMax(gpu.device, 0, patch::GEOMETRY_BYTES));
}
