#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <webgpu/webgpu.h>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "ipass/ipass.h"

#include <vector>
#include <cstring>

class PipelineBinding {
    ipass::Pipeline* pipeline = nullptr;

    static emscripten::val webgpu() {
        return emscripten::val::module_property("WebGPU");
    }

public:
    PipelineBinding(emscripten::val jsDevice, uint32_t maxPatches) {
        uintptr_t h = webgpu().call<uintptr_t>("importJsDevice", jsDevice);
        wgpu::Device device(reinterpret_cast<WGPUDevice>(h));
        ipass::Config config;
        config.max_patches = maxPatches;
        pipeline = new ipass::Pipeline(device, config);
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

    emscripten::val getVertexBuffer() {
        wgpu::Buffer buf = pipeline->GetVertexBuffer();
        uintptr_t h = reinterpret_cast<uintptr_t>(static_cast<WGPUBuffer>(buf));
        return webgpu().call<emscripten::val>("getJsObject", h);
    }

    emscripten::val getLODBuffer() {
        wgpu::Buffer buf = pipeline->GetLODBuffer();
        uintptr_t h = reinterpret_cast<uintptr_t>(static_cast<WGPUBuffer>(buf));
        return webgpu().call<emscripten::val>("getJsObject", h);
    }

    uint32_t getMaxVertexCount() {
        return pipeline->GetMaxVertexCount();
    }
};

emscripten::val loadBVFile(const std::string& filepath, uint32_t maxPatches) {
    ipass::Status status;
    ipass::PatchData data = (maxPatches > 0)
        ? ipass::BVLoader::Load(filepath, maxPatches, &status)
        : ipass::BVLoader::Load(filepath, &status);

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
    emscripten::class_<PipelineBinding>("_Pipeline")
        .constructor<uintptr_t, uint32_t>()
        .function("loadPatches", &PipelineBinding::loadPatches)
        .function("setMVP", &PipelineBinding::setMVP)
        .function("setViewport", &PipelineBinding::setViewport)
        .function("execute", &PipelineBinding::execute)
        .function("dispatchLOD", &PipelineBinding::dispatchLOD)
        .function("dispatchTessellation", &PipelineBinding::dispatchTessellation)
        .function("getVertexBuffer", &PipelineBinding::getVertexBuffer)
        .function("getLODBuffer", &PipelineBinding::getLODBuffer)
        .function("getMaxVertexCount", &PipelineBinding::getMaxVertexCount);

    function("_loadBVFile", &loadBVFile);

    emscripten::constant("FLOATS_PER_VERTEX", ipass::OutputVertexLayout::FLOATS_PER_VERTEX);
    emscripten::constant("BYTES_PER_VERTEX", ipass::OutputVertexLayout::BYTES_PER_VERTEX);
    emscripten::constant("POSITION_OFFSET", ipass::OutputVertexLayout::POSITION_OFFSET);
    emscripten::constant("NORMAL_OFFSET", ipass::OutputVertexLayout::NORMAL_OFFSET);
    emscripten::constant("COLOR_OFFSET", ipass::OutputVertexLayout::COLOR_OFFSET);
    emscripten::constant("UV_OFFSET", ipass::OutputVertexLayout::UV_OFFSET);


    emscripten::enum_<ipass::Status>("Status")
        .value("Success", ipass::Status::Success)
        .value("EmptyInput", ipass::Status::EmptyInput)
        .value("GPUInitFailed", ipass::Status::GPUInitFailed)
        .value("NotInitialized", ipass::Status::NotInitialized)
        .value("PatchesNotLoaded", ipass::Status::PatchesNotLoaded)
        .value("LoadFailed", ipass::Status::LoadFailed);
}
