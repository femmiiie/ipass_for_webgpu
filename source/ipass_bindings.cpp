#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <webgpu/webgpu.h>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "ipass/ipass.h"

#include <vector>
#include <cstring>

static emscripten::val webgpu() {
    return emscripten::val::module_property("WebGPU");
}

static emscripten::val toJs(void* handle) {
    return webgpu().call<emscripten::val>("getJsObject", reinterpret_cast<uintptr_t>(handle));
}

template <class P>
class PipelineBinding {
    P* pipeline = nullptr;

public:
    PipelineBinding(emscripten::val jsDevice, uint32_t maxPatches) {
        uintptr_t h = webgpu().call<uintptr_t>("importJsDevice", jsDevice);
        wgpu::Device device(reinterpret_cast<WGPUDevice>(h));
        pipeline = new P(device, maxPatches);
    }

    ~PipelineBinding() {
        delete pipeline;
    }

    int loadPatches(emscripten::val controlPointsF32, emscripten::val cornerIndicesU32, uint32_t numPatches) {
        ipass::PatchData data;
        data.num_patches = numPatches;

        auto cpVec = convertJSArrayToNumberVector<float>(controlPointsF32);
        data.control_points.resize(numPatches * 16);
        std::memcpy(data.control_points.data(), cpVec.data(),
            std::min(cpVec.size(), static_cast<size_t>(numPatches * 16 * 4)) * sizeof(float));

        data.corner_indices = convertJSArrayToNumberVector<uint32_t>(cornerIndicesU32);

        return static_cast<int>(pipeline->LoadPatches(data));
    }

    void setMVP(emscripten::val mvpF32) {
        auto mvpVec = convertJSArrayToNumberVector<float>(mvpF32);
        glm::mat4 mvp;
        std::memcpy(&mvp, mvpVec.data(), sizeof(glm::mat4));
        pipeline->SetMVP(mvp);
    }

    void setViewport(float width, float height) {
        pipeline->SetViewport(width, height);
    }

    int execute(emscripten::val jsEncoder) {
        uintptr_t h = webgpu().call<uintptr_t>("importJsCommandEncoder", jsEncoder);
        wgpu::CommandEncoder encoder(reinterpret_cast<WGPUCommandEncoder>(h));
        return static_cast<int>(pipeline->Execute(encoder));
    }

    int dispatchLOD(emscripten::val jsEncoder) {
        uintptr_t h = webgpu().call<uintptr_t>("importJsCommandEncoder", jsEncoder);
        wgpu::CommandEncoder encoder(reinterpret_cast<WGPUCommandEncoder>(h));
        return static_cast<int>(pipeline->DispatchLOD(encoder));
    }

    int dispatchTessellation(emscripten::val jsEncoder) {
        uintptr_t h = webgpu().call<uintptr_t>("importJsCommandEncoder", jsEncoder);
        wgpu::CommandEncoder encoder(reinterpret_cast<WGPUCommandEncoder>(h));
        return static_cast<int>(pipeline->DispatchTessellation(encoder));
    }

    emscripten::val getLODBuffer() {
        return toJs(static_cast<WGPUBuffer>(pipeline->GetLODBuffer()));
    }


    emscripten::val getVertexBuffer() {
        return toJs(static_cast<WGPUBuffer>(pipeline->GetVertexBuffer()));
    }

    uint32_t getMaxVertexCount() {
        return pipeline->GetMaxVertexCount();
    }

    int draw(emscripten::val jsPass) {
        uintptr_t h = webgpu().call<uintptr_t>("importJsRenderPassEncoder", jsPass);
        wgpu::RenderPassEncoder pass(reinterpret_cast<WGPURenderPassEncoder>(h));
        return static_cast<int>(pipeline->Draw(pass));
    }

    emscripten::val getSurfaceBindGroupLayout() {
        return toJs(static_cast<WGPUBindGroupLayout>(pipeline->GetSurfaceBindGroupLayout()));
    }

    static std::string getSurfaceShaderSource() {
        return P::GetSurfaceShaderSource();
    }
};

using GeometryBinding = PipelineBinding<ipass::ComputePipeline>;
using DirectBinding   = PipelineBinding<ipass::DirectPipeline>;

// the methods both pipelines share
template <class B>
void bindShared(const emscripten::class_<B>& c) {
    c.template constructor<emscripten::val, uint32_t>()
        .function("loadPatches", &B::loadPatches)
        .function("setMVP", &B::setMVP)
        .function("setViewport", &B::setViewport)
        .function("execute", &B::execute)
        .function("dispatchLOD", &B::dispatchLOD)
        .function("dispatchTessellation", &B::dispatchTessellation)
        .function("getLODBuffer", &B::getLODBuffer);
}

emscripten::val loadBVFile(const std::string& filepath, uint32_t maxPatches) {
    ipass::Status status;
    ipass::PatchData data = ipass::LoadBV(filepath, &status, maxPatches);

    emscripten::val result = emscripten::val::object();
    result.set("status", static_cast<int>(status));

    if (status == ipass::Status::Success) {
        size_t cpFloats = data.num_patches * 16 * 4;
        emscripten::val cpArray = emscripten::val::global("Float32Array").new_(cpFloats);
        emscripten::val cpView = emscripten::val(emscripten::typed_memory_view(cpFloats,
            reinterpret_cast<const float*>(data.control_points.data())));
        cpArray.call<void>("set", cpView);
        result.set("controlPoints", cpArray);

        emscripten::val ciArray = emscripten::val::global("Uint32Array").new_(data.corner_indices.size());
        emscripten::val ciView = emscripten::val(emscripten::typed_memory_view(data.corner_indices.size(),
            data.corner_indices.data()));
        ciArray.call<void>("set", ciView);
        result.set("cornerIndices", ciArray);

        result.set("numPatches", data.num_patches);
    }

    return result;
}

EMSCRIPTEN_BINDINGS(ipass_module) {
    bindShared(emscripten::class_<GeometryBinding>("_GeometryPipeline")
        .function("getVertexBuffer", &GeometryBinding::getVertexBuffer)
        .function("getMaxVertexCount", &GeometryBinding::getMaxVertexCount));

    bindShared(emscripten::class_<DirectBinding>("_DirectPipeline")
        .function("draw", &DirectBinding::draw)
        .function("getSurfaceBindGroupLayout", &DirectBinding::getSurfaceBindGroupLayout)
        .class_function("getSurfaceShaderSource", &DirectBinding::getSurfaceShaderSource));

    function("_loadBVFile", &loadBVFile);

    emscripten::constant("SURFACE_BIND_GROUP", ipass::SURFACE_BIND_GROUP);
    emscripten::constant("FLOATS_PER_VERTEX", ipass::OutputVertexLayout::FLOATS_PER_VERTEX);
    emscripten::constant("BYTES_PER_VERTEX", ipass::OutputVertexLayout::BYTES_PER_VERTEX);
    emscripten::constant("POSITION_OFFSET", ipass::OutputVertexLayout::POSITION_OFFSET);
    emscripten::constant("NORMAL_OFFSET", ipass::OutputVertexLayout::NORMAL_OFFSET);
    emscripten::constant("UV_OFFSET", ipass::OutputVertexLayout::UV_OFFSET);
    emscripten::constant("PATCH_INDEX_OFFSET", ipass::OutputVertexLayout::PATCH_INDEX_OFFSET);
    emscripten::constant("BARY_ID_OFFSET", ipass::OutputVertexLayout::BARY_ID_OFFSET);


    emscripten::enum_<ipass::Status>("Status")
        .value("Success", ipass::Status::Success)
        .value("EmptyInput", ipass::Status::EmptyInput)
        .value("GPUInitFailed", ipass::Status::GPUInitFailed)
        .value("NotInitialized", ipass::Status::NotInitialized)
        .value("PatchesNotLoaded", ipass::Status::PatchesNotLoaded)
        .value("LoadFailed", ipass::Status::LoadFailed);
}
