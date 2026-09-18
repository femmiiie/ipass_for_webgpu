# About this project

iPASS is a GPU tessellation pipeline for bicubic patches on top of WebGPU.
It consists of 2 main sections: a compute shader that determines the correct tessellation level for pixel-accuracy, and a compute shader tessellation engine, modelling the OpenGL tessellation scheme.

For C++ users, the typical flow is:

1. Create an [`ipass::Pipeline`](components/Pipeline#class-pipeline) with your `wgpu::Device` and `wgpu::Queue`
2. Load patch data (from a BV file with [`ipass::LoadBV`](components/BV-Loader) or from your own data)
3. Dispatch the LOD + tessellation compute passes
4. Use the generated vertex buffer in your render pass

The same flow is available from JavaScript through the [WASM module](components/JS-Pipeline).

See [Getting Started](getting-started) for setup instructions and code examples.
