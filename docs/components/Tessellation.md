# Tessellation Pass

Tessellates patches into triangle meshes on the GPU.

## Public Types

### `class TessellationPass`

Standalone compute pass that generates tessellated vertex data from patch control points and LOD values.

**Constructors**

- `TessellationPass(wgpu::Device device, const` [`Config`](PatchData#struct-config)`& config = {})`: Creates a tessellation pass using the given device and optional configuration. Internally obtains the queue from the device.
- `TessellationPass(wgpu::Device device, wgpu::Queue queue, const` [`Config`](PatchData#struct-config)`& config = {})`: Creates a tessellation pass using an explicit device and queue pair.

**Copy/Move**

- **Copy** is *not* supported
- **Move** is supported

**Public Methods**

- [`Status`](PatchData#enum-status) ` UploadPatches(const` [`PatchData`](PatchData#struct-patchdata)`& data, wgpu::Buffer lod_buffer)`: Uploads patch data and binds an external LOD buffer for tessellation.
- [`Status`](PatchData#enum-status) ` Dispatch(wgpu::CommandEncoder& encoder)`: Records the tessellation compute pass into the command encoder.
- `wgpu::Buffer GetVertexBuffer() const`: Returns the GPU buffer containing the tessellated vertex output.
- `uint32_t GetMaxVertexCount() const`: Returns the maximum number of vertices the output buffer can hold.
- `uint32_t GetPatchCount() const`: Returns the number of patches currently loaded.