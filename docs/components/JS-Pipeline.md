# Pipeline (JavaScript)

Main JavaScript/WASM module interface. Allows management of LOD calculation and tessellation per object.
Wraps the C++ [`Pipeline`](Pipeline#class-pipeline); unlike the C++ API, the standalone LOD and tessellation
passes are not individually exposed to JavaScript.

## Public Types

### `class Pipeline`

Wrapper to manage the full patch compute pipeline. Exposed on the loaded module as `Module.Pipeline`.

**Constructor**

- `new Pipeline(device: GPUDevice, options?:` [`PipelineOptions`](JS-PatchData#interface-pipelineoptions)`)`: Creates a pipeline using the given `GPUDevice` and optional configuration.

**Public Methods**

- `loadPatches(data:` [`PatchData`](JS-PatchData#interface-patchdata)`): number`: Uploads patch data to the GPU for processing. Returns a [`Status`](JS-PatchData#enum-status) value.
- `setMVP(matrix: Float32Array | number[]): void`: Sets the model-view-projection matrix used for LOD calculations.
- `setViewport(width: number, height: number): void`: Sets the viewport dimensions used for LOD calculations.
- `dispatchLOD(encoder: GPUCommandEncoder): number`: Records the LOD compute pass into the command encoder. Returns a [`Status`](JS-PatchData#enum-status) value.
- `dispatchTessellation(encoder: GPUCommandEncoder): number`: Records the tessellation compute pass into the command encoder. Returns a [`Status`](JS-PatchData#enum-status) value.
- `execute(encoder: GPUCommandEncoder): number`: Records both the LOD and tessellation passes into the command encoder in order. Returns a [`Status`](JS-PatchData#enum-status) value.
- `getVertexBuffer(): GPUBuffer`: Returns the GPU buffer containing the tessellated vertex output.
- `getLODBuffer(): GPUBuffer`: Returns the GPU buffer containing the computed LOD values.
- `getMaxVertexCount(): number`: Returns the maximum number of vertices the output buffer can hold.
- `destroy(): void`: Releases the underlying WASM object. Must be called once the pipeline is no longer needed.
