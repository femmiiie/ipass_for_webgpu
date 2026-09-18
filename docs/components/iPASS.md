# ipass

Umbrella header that includes the entire library.

## Includes

- [`PatchData.h`](PatchData): Core data types, configuration, and status codes.
- [`LODPass.h`](LOD): Standalone LOD compute pass.
- [`TessellationPass.h`](Tessellation): Standalone tessellation compute pass.
- [`Pipeline.h`](Pipeline): High-level pipeline combining LOD and tessellation.

## Public Functions

- [`PatchData`](PatchData#struct-patchdata) ` LoadBV(const std::string& filepath,` [`Status`](PatchData#enum-status)`* status = nullptr, uint32_t max_patches = 0)`: Loads patches from a `.bv` file. See [BV Loader](BV-Loader).