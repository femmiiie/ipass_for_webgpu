# Patch Data

Core data types used to configure the library.

## Public Types

### `enum Status`

Signals the result of a library operation

**Values**
- `Status::Success`: The operation was successful.
- `Status::EmptyInput`: The operation was unsuccessful due to a lack of input data.
- `GPUInitFailed`: The operation was unsuccessful due to an error with the GPU. Ensure you have enough VRAM free.
- `NotInitialized`: The operation was unsuccessful because the library isn't initialized yet.
- `PatchesNotLoaded`: The operation was unsuccessful because patch data hasn't been loaded yet.
- `LoadFailed`: The data-loading operation failed.

---

### `struct PatchData`

Used to input the patch data into the library

**Fields**
- `std::vector<glm::vec4> control_points`: Flattened array of patch control points (16 per patch).
- `std::vector<uint32_t> corner_indices`: Flattened array of patch corner indices (4 per patch).
-  `uint32_t num_patches`: Total number of patches.

**Requirements**
- `control_points.size() = num_patches * 16`
- `corner_indices.size() = num_patches * 4`

---

### `struct OutputVertexLayout`

Contains constant information about the library instance

**Fields (All Constant)**

- `uint32_t FLOATS_PER_VERTEX`: number of floating-point values per vertex
- `uint32_t BYTES_PER_VERTEX`: size of a vertex in bytes (generally 4 * FLOATS_PER_VERTEX)
- `uint32_t POSITION_OFFSET`: byte offset of the position attribute in the vertex
- `uint32_t NORMAL_OFFSET`: byte offset of the normal attribute in the vertex
- `uint32_t COLOR_OFFSET`: byte offset of the color attribute in the vertex
- `uint32_t UV_OFFSET`: byte offset of the UV attribute in the vertex

---

### `struct Config`

Configuration used in the library initialization

**Fields**
- `uint32_t max_patches`: max number of patches to process
- `const char* shader_base_path` shader directory