# Patch Data (JavaScript)

Core data types used to configure the JavaScript/WASM module. Mirrors the [C++ Patch Data](PatchData) types.

## Public Types

### `enum Status`

Signals the result of a library operation. Numeric values match the C++ [`ipass::Status`](PatchData#enum-status) enum.

**Values**
- `Status.Success` (`0`): The operation was successful.
- `Status.EmptyInput` (`1`): The operation was unsuccessful due to a lack of input data.
- `Status.GPUInitFailed` (`2`): The operation was unsuccessful due to an error with the GPU. Ensure you have enough VRAM free.
- `Status.NotInitialized` (`3`): The operation was unsuccessful because the library isn't initialized yet.
- `Status.PatchesNotLoaded` (`4`): The operation was unsuccessful because patch data hasn't been loaded yet.
- `Status.LoadFailed` (`5`): The data-loading operation failed.

---

### `interface PipelineOptions`

Options passed to the [`Pipeline`](JS-Pipeline#class-pipeline) constructor.

**Fields**
- `maxPatches?: number`: max number of patches to process. `0` or omitted autosizes from the device limit.

---

### `interface PatchData`

Used to input patch data into the [`Pipeline`](JS-Pipeline#class-pipeline).

**Fields**
- `controlPoints: Float32Array`: Flattened array of patch control points (16 `vec4`s per patch).
- `cornerIndices: Uint32Array`: Flattened array of patch corner indices (4 per patch).
- `numPatches: number`: Total number of patches.

---

### `interface BVLoadResult`

Result returned by [`loadBVFile`](JS-BVLoader#function-loadbvfile).

**Fields**
- `status: number`: One of the [`Status`](#enum-status) values.
- `controlPoints: Float32Array`: Loaded control points.
- `cornerIndices: Uint32Array`: Loaded corner indices.
- `numPatches: number`: Total number of patches loaded.
- `patchData(): PatchData`: Returns the loaded data as a [`PatchData`](#interface-patchdata) object, ready to pass to `loadPatches` on [`Pipeline`](JS-Pipeline#class-pipeline).

---

### `interface OutputVertexLayout`

Contains constant information about the layout of the tessellated vertex output. Mirrors the C++ [`OutputVertexLayout`](PatchData#struct-outputvertexlayout) struct.

**Fields (All Constant)**

- `FLOATS_PER_VERTEX: 16`: number of floating-point values per vertex.
- `BYTES_PER_VERTEX: 64`: size of a vertex in bytes.
- `POSITION_OFFSET: 0`: byte offset of the position attribute in the vertex.
- `NORMAL_OFFSET: 16`: byte offset of the normal attribute in the vertex.
- `COLOR_OFFSET: 32`: byte offset of the color attribute in the vertex.
- `UV_OFFSET: 48`: byte offset of the UV attribute in the vertex.
