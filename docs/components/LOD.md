# LOD Pass
 
Computes per-patch level-of-detail values on the GPU.
 
## Public Types
 
### `class LODPass`
 
Standalone compute pass that determines the tessellation level for each patch based on the current view.
 
**Constructors**
 
- `LODPass(wgpu::Device device, const` [`Config`](PatchData#struct-config)`& config = {})`: Creates an LOD pass using the given device and optional configuration.
- `LODPass(wgpu::Device device, wgpu::Queue queue, const` [`Config`](PatchData#struct-config)`& config = {})`: Creates an LOD pass using an explicit device and queue pair.
 
**Copy/Move**
 
- **Copy** is *not* supported
- **Move** is supported
 
**Public Methods**
 
- [`Status`](PatchData#enum-status) ` UploadPatches(const` [`PatchData`](PatchData#struct-patchdata)`& data)`: Uploads patch data to the GPU for LOD computation.
- `void SetMVP(const glm::mat4& mvp)`: Sets the model-view-projection matrix used for LOD calculations.
- `void SetViewport(float width, float height)`: Sets the viewport dimensions used for LOD calculations.
- [`Status`](PatchData#enum-status) ` Dispatch(wgpu::CommandEncoder& encoder)`: Records the LOD compute pass into the command encoder.
- `wgpu::Buffer GetLODBuffer() const`: Returns the GPU buffer containing the computed LOD values.
- `uint32_t GetPatchCount() const`: Returns the number of patches currently loaded.
 