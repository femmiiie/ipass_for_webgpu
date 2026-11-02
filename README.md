# iPASS For WebGPU Library

## Table of Contents

1. [Overview](#overview)
2. [Building](#building)
   - [C++](#c)
   - [Javascript](#javascript)
3. [Getting Started](#getting-started)
   - [C++](#c-1)
     - [Compute Pipeline](#compute-pipeline)
     - [Direct Pipeline](#direct-pipeline)
   - [Javascript](#javascript-1)
     - [Compute Pipeline](#compute-pipeline-1)
     - [Direct Pipeline](#direct-pipeline-1)
4. [Dependencies](#dependencies)

## Overview

iPASS is a GPU tessellation pipeline for bicubic patches on top of WebGPU.\
It consists of 2 main sections: a compute shader that determines the correct tessellation level for pixel-accuracy, and a tessellation engine, modelling the OpenGL tessellation scheme.

Two pipelines are provided, differing only in where the surface is evaluated:

- **`ComputePipeline`**: a compute shader evaluates every tessellated vertex into a vertex/storage buffer, which you draw like any other mesh.
  Use it when you need the output geometry itself (further compute passes, readback, export). Output space is reserved for the worst case
  tessellation of every patch, so fewer patches fit in the device limits, and each draw covers that worst case vertex count.
- **`DirectPipeline`**: your vertex shader calls `ipass_vertex` to evaluate each point at draw time, driven by an indirect draw.
  Only per patch levels and draw arguments are stored, so far more patches fit, but the surface is evaluated again in every pass that draws it.

For C++ users, the typical flow is:

1. Create an `ipass::ComputePipeline` or `ipass::DirectPipeline` with your `wgpu::Device` and `wgpu::Queue`
2. Load patch data (from a BV file with `ipass::LoadBV` or from your own data)
3. Build your render pipeline
   - `ComputePipeline`: with a vertex buffer layout matching the [output vertex](#compute-pipeline)
   - `DirectPipeline`: with the WGSL from `DirectPipeline::GetSurfaceShaderSource()` added to your shader, and its bind group layout at `ipass::SURFACE_BIND_GROUP`
4. Each frame, set the MVP and viewport, then call `Execute` to dispatch the LOD + tessellation compute passes
5. Draw the surface
   - `ComputePipeline`: bind `GetVertexBuffer()` and draw `GetMaxVertexCount()` vertices
   - `DirectPipeline`: call `Draw` on your render pass

## Building

Clone the repository using `git clone https://github.com/femmiiie/ipass_for_webgpu.git`.

### C++

#### Prerequisites

- **CMake** >= 3.5
- **C++23 compatible compiler**

```bash
cmake -B build
cmake --build build --config Release
```

The WebGPU backend can be specified with `-DBACKEND`. Available options are `DAWN` and `WGPU`.

Tests are off by default, enable them with `-DIPASS_BUILD_TESTS=ON` and run with `ctest --test-dir build -C Release`.

### Javascript

#### Prerequisites

- **CMake** >= 3.5
- **C++23 compatible compiler**
- [**Emscripten SDK**](https://emscripten.org/docs/getting_started/downloads.html)
  - Ensure `emsdk` is in your PATH and initialized (the `emcmake` command should be working)

```bash
emcmake cmake -B build_wasm
cmake --build build_wasm --config Release --target ipass_wasm
```

The output files will be `ipass.js` and `ipass.wasm`. Include these files and use in your project accordingly.

## Getting Started

### C++

#### Add Project to CMake

```cmake
add_subdirectory(LIBRARY_PATH)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE ipass_lib)
target_compile_features(my_app PRIVATE cxx_std_23)
```

#### Loading Patches

Both pipelines are constructed and loaded the same way.

```cpp
#include <ipass/ipass.h>

//optional max_patches as last argument, 0 autosizes based on device limits
ipass::ComputePipeline pipeline(device, queue);
//or
ipass::DirectPipeline pipeline(device, queue);

ipass::Status loadStatus;
ipass::PatchData patches = ipass::LoadBV("input.bv", &loadStatus);
if (loadStatus != ipass::Status::Success)
  return;

if (pipeline.LoadPatches(patches) != ipass::Status::Success)
  return;
```

#### Camera and Viewport

The LOD pass picks tessellation levels for pixel accuracy, so it needs the same transform you render with and the viewport
size in pixels. Both are shared by the two pipelines and only need to be set again when they change.

```cpp
glm::mat4 M = glm::mat4(1.0f);
glm::mat4 V = glm::lookAt( //camera at z = 3 looking at the origin
  glm::vec3(0.0f, 0.0f, 3.0f),
  glm::vec3(0.0f, 0.0f, 0.0f),
  glm::vec3(0.0f, 1.0f, 0.0f)
);
glm::mat4 P = glm::perspective(glm::radians(45.0f), width / height, 0.1f, 100.0f);

pipeline.SetMVP(P * V * M);
pipeline.SetViewport(width, height);
```

#### Compute Pipeline

`ipass::ComputePipeline` writes the tessellated surface into a vertex buffer as a non indexed triangle list. Each vertex is 3 `vec4f` (see `ipass::OutputVertexLayout`), and is structured as follows in WGSL:

```wgsl
struct VertexIn {
  @location(0) position: vec4f,
  @location(1) normal:   vec4f,
  // uv, patch index, corner index
  @location(2) extra:    vec4f, 
}
```

`Execute` records the LOD and tessellation compute passes into your encoder, so the render pass can follow in the same
command buffer.

```cpp
wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

if (pipeline.Execute(encoder) != ipass::Status::Success) {
  encoder.release();
  return;
}

wgpu::Buffer vertexBuffer = pipeline.GetVertexBuffer();

wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
renderPass.setPipeline(renderPipeline);
renderPass.setBindGroup(0, myBindGroup, 0, nullptr);
renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexBuffer.getSize());
renderPass.draw(pipeline.GetMaxVertexCount(), 1, 0, 0);
renderPass.end();
renderPass.release();

wgpu::CommandBufferDescriptor commandDesc = wgpu::Default;
wgpu::CommandBuffer command = encoder.finish(commandDesc);
encoder.release();

queue.submit(1, &command);
command.release();
```

The vertex buffer also has `Storage` usage, so it can be bound directly in your own compute passes. `GetTriCountBuffer()`
holds the total number of triangles written by the last `Execute`.

#### Direct Pipeline

`ipass::DirectPipeline` evaluates the surface in your own vertex shader, so no vertex buffer is written. Append
`ipass::DirectPipeline::GetSurfaceShaderSource()` to your shader source and call `ipass_vertex` from a vertex entry point
with no vertex buffers:

```wgsl
@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  // position, normal, uv, patch_idx, bary_id
  let v = ipass_vertex(vid, iid);
  ...
}
```

Bind group 1 (`ipass::SURFACE_BIND_GROUP`) is reserved for the surface data, so add its layout at that index of your
pipeline layout. It is available before `LoadPatches`, so render pipelines can be built up front.

```cpp
std::string shaderSource = myShaderSource + "\n" + ipass::DirectPipeline::GetSurfaceShaderSource();
WGPUBindGroupLayout bindGroupLayouts[2] = { myBindGroupLayout, pipeline.GetSurfaceBindGroupLayout() };
```

Each frame matches the compute pipeline, except the draw:

```cpp
renderPass.setPipeline(renderPipeline);
renderPass.setBindGroup(0, myBindGroup, 0, nullptr);
pipeline.Draw(renderPass); //binds the surface group and calls drawIndirect
```

`Draw` can be called in any number of render passes after a single `Execute` (for example a depth prepass or shadow pass),
but the surface is evaluated again in each one.

### Javascript

The build is an Emscripten module, `createIPASS` loads it and resolves once the wasm is ready.

```html
<script src="path/to/ipass.js"></script>
<!-- ipass.wasm is loaded automatically by ipass.js -->
```

#### Loading Patches

```javascript
const ipass = await createIPASS();

// optional maxPatches as the last argument, 0 autosizes based on device limits
const pipeline = new ipass.ComputePipeline(device);
// or
const pipeline = new ipass.DirectPipeline(device);

const bvBuffer = await fetch("assets/model.bv").then(r => r.arrayBuffer());
const loadResult = ipass.loadBVFile(bvBuffer);

if (loadResult.status !== ipass.Status.Success ||
    pipeline.loadPatches(loadResult.patchData()) !== ipass.Status.Success) {
  pipeline.destroy();
  return;
}

// main application loop

pipeline.destroy();
```

#### Camera and Viewport

Matrices are column major, as produced by [gl-matrix](https://glmatrix.net/). WebGPU clip space depth is `0..1`, so use
`perspectiveZO`.

```javascript
const model = mat4.create();
const view  = mat4.lookAt(mat4.create(), [0, 0, 3], [0, 0, 0], [0, 1, 0]); // camera at z = 3 looking at the origin
const proj  = mat4.perspectiveZO(mat4.create(), Math.PI / 4, canvas.width / canvas.height, 0.1, 100.0);

const mvp = mat4.create();
mat4.multiply(mvp, proj, view);
mat4.multiply(mvp, mvp, model);

pipeline.setMVP(mvp);
pipeline.setViewport(canvas.width, canvas.height);
```

#### Compute Pipeline

The output is the same as the [C++ pipeline](#compute-pipeline), and `ipass.OutputVertexLayout` describes it for your
vertex buffer layout:

```javascript
const L = ipass.OutputVertexLayout;
const vertexBufferLayout = {
  arrayStride: L.BYTES_PER_VERTEX,
  attributes: [
    { shaderLocation: 0, offset: L.POSITION_OFFSET, format: "float32x4" },
    { shaderLocation: 1, offset: L.NORMAL_OFFSET,   format: "float32x4" },
    // uv, patch index, corner index
    { shaderLocation: 2, offset: L.UV_OFFSET,       format: "float32x4" },
  ],
};
```

```javascript
const encoder = device.createCommandEncoder();
if (pipeline.execute(encoder) !== ipass.Status.Success)
  return;

const renderPass = encoder.beginRenderPass(renderPassDescriptor);
renderPass.setPipeline(renderPipeline);
renderPass.setBindGroup(0, myBindGroup);
renderPass.setVertexBuffer(0, pipeline.getVertexBuffer());
renderPass.draw(pipeline.getMaxVertexCount());
renderPass.end();

device.queue.submit([encoder.finish()]);
```

#### Direct Pipeline

Append `ipass.DirectPipeline.getSurfaceShaderSource()` to your shader, call `ipass_vertex` as in the
[C++ example](#direct-pipeline), and add the surface layout at `ipass.SURFACE_BIND_GROUP` (1) of your pipeline layout:

```javascript
const shaderModule = device.createShaderModule({
  code: shaderSource + "\n" + ipass.DirectPipeline.getSurfaceShaderSource(),
});
const pipelineLayout = device.createPipelineLayout({
  bindGroupLayouts: [myBindGroupLayout, pipeline.getSurfaceBindGroupLayout()],
});
```

Each frame matches the compute pipeline, except the draw:

```javascript
renderPass.setPipeline(renderPipeline);
renderPass.setBindGroup(0, myBindGroup);
pipeline.draw(renderPass); // binds the surface group and calls drawIndirect
```

## Dependencies

All dependencies are pulled automatically using CMake's FetchContent.

- [glm](https://github.com/g-truc/glm)
- [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution)
