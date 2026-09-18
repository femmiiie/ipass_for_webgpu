# Pipeline

Main library interface. Allows management of LOD calculation and tessellation per object.

## Public Types

### `class Pipeline`

Wrapper to manage the full patch compute pipeline.

**Constructors**

- `Pipeline(wgpu::Device device, const` [`Config`](PatchData#struct-config)`& config = {})`: Creates a pipeline using the given device and optional configuration.
- `Pipeline(wgpu::Device device, wgpu::Queue queue, const` [`Config`](PatchData#struct-config)`& config = {})`: Creates a pipeline using an explicit device and queue pair.

**Copy/Move**

- **Copy** is *not* supported
- **Move** is supported

**Public Methods**
- [`Status`](PatchData#enum-status) ` LoadPatches(const` [`PatchData`](PatchData#struct-patchdata)`& data)`: Uploads patch data to the GPU for processing.
- `void SetMVP(const glm::mat4& mvp)`: Sets the model-view-projection matrix used for LOD calculations.
- `void SetViewport(float width, float height)`: Sets the viewport dimensions used for LOD calculations.
- [`Status`](PatchData#enum-status) ` DispatchLOD(wgpu::CommandEncoder& encoder)`: Records the LOD compute pass into the command encoder.
- [`Status`](PatchData#enum-status) ` DispatchTessellation(wgpu::CommandEncoder& encoder)`: Records the tessellation compute pass into the command encoder.
- [`Status`](PatchData#enum-status) ` Execute(wgpu::CommandEncoder& encoder)`: Records both the LOD and tessellation passes into the command encoder in order.
- `wgpu::Buffer GetVertexBuffer() const`: Returns the GPU buffer containing the tessellated vertex output.
- `uint32_t GetMaxVertexCount() const`: Returns the maximum number of vertices the output buffer can hold.
- `wgpu::Buffer GetLODBuffer() const`: Returns the GPU buffer containing the computed LOD values.
- [`LODPass`](LOD#class-lodpass)`& GetLOD()`: Returns a reference to the internal LOD pass for direct configuration.
- [`TessellationPass`](Tessellation#class-tessellationpass)`& GetTessellationPass()`: Returns a reference to the internal tessellation pass for direct configuration.