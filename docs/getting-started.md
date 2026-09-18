# Getting Started

## Building

Building the main app with CMake builds the library by default, below is instructions for building the library separately.
Clone the repository using `git clone https://github.com/femmiiie/senior_project.git`.

### C++

#### Prerequisites

- **CMake** >= 3.5
- **C++23 compatible compiler**

```bash
cmake -B build
cmake --build build --config Release --target ipass_lib
```

The WebGPU backend can be specified with `-DBACKEND`. Available options are `DAWN` and `WGPU`.

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

## Code Examples

### C++

#### Add Project to CMake

```cmake
add_subdirectory(LIBRARY_PATH)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE ipass_lib)
target_compile_features(my_app PRIVATE cxx_std_23)
```

### Initializing Pipeline

```cpp
#include <ipass/ipass.h>

ipass::Config config; //optional
ipass::Pipeline pipeline(device, queue, config);

ipass::Status loadStatus;
ipass::PatchData patches = ipass::BVLoader::Load("input.bv":, &loadStatus);
if (loadStatus != ipass::Status::Success)
  return;

if (pipeline.LoadPatches(patches) != ipass::Status::Success)
  return;
```

#### Running Pipeline

```cpp
//basic glm MVP code
glm::mat4 M = glm::mat4(1.0f);
glm::mat4 V = glm::lookAt( //camera centered at origin looking down x-axis
  {0.0f, 0.0f, 0.0f,},
  {1.0f, 0.0f, 0.0f},
  {0.0f, 1.0f, 0.0f}
)
glm::mat4 P = glm::perspective(glm::radians(45.0f), /* width / height */, 0.1, 100.0);

if MVP_DIFFERENT
  pipeline.SetMVP(P * V * M); 

if VIEWPORT_DIFFERENT
  pipeline.SetViewport(/*width*/, /*height*/);

wgpu::CommandEncoderDescriptor encoderDescriptor;
encoderDesc.label = WGPU_STRING_VIEW_INIT;
wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDescriptor);

if (pipeline.Execute(encoder) != ipass::Status::Success) {
   encoder.release();
   return;
}

wgpu::CommandBufferDescriptor commandDescriptor;
commandDescriptor.label = WGPU_STRING_VIEW_INIT;
wgpu::CommandBuffer command = encoder.finish(commandDescriptor);
encoder.release();

queue.submit(1, &command);
command.release();

wgpu::Buffer tessVertexBuffer = pipeline.GetVertexBuffer();
uint32_t maxVertices = pipeline.GetMaxVertexCount();
// continue with rendering pipeline...
```

### Javascript

#### Minimal Pipeline Setup

In your HTML file:

```html
<script src="path/to/ipass.js"></script>
<!-- ipass.wasm is loaded automatically by ipass.js -->
```

```javascript
const pipeline = new Module.Pipeline(device, { maxPatches: 0 }); // 0 = auto

const bvBuffer = await fetch("assets/model.bv").then(r => r.arrayBuffer());
const loadResult = Module.loadBVFile(bvBuffer, { maxPatches: 0 });
    
if (loadResult.status !== Module.Status.Success) {
  pipeline.destroy();
  return;
}

const loadStatus = pipeline.loadPatches(loadResult.patchData());
if (loadStatus !== Module.Status.Success) {
  pipeline.destroy();
  return;
}

// main application loop

pipeline.destroy();
```

#### Running Pipeline

```javascript
const model = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]; // identity
const view = mat4.lookAt( //camera centered at origin looking down x-axis
    [0, 0, 3],
    [0, 0, 0],
    [0, 1, 0]
);
const proj = mat4.perspective(Math.PI / 4, /* width / height */, 0.1, 100.0);
const mvp = mat4.multiply(proj, mat4.multiply(view, model));
pipeline.setMVP(new Float32Array(mvp));
    
pipeline.setViewport(/*width*/, /*height*/);

const encoder = device.createCommandEncoder();
if (pipeline.execute(encoder) !== Module.Status.Success) {
  encoder.finish().release();
  pipeline.destroy();
  return;
}

const commandBuffer = encoder.finish();
queue.submit([commandBuffer]);

const tessVertexBuffer = pipeline.getVertexBuffer();
const maxVertices = pipeline.getMaxVertexCount();
// continue with rendering pipeline...
```
