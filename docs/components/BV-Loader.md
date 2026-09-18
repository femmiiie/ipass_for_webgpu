# BV Loader

Utility for loading patch data from `.bv` files.

## Public Functions

### `namespace BVLoader`

Free functions for parsing `.bv` files into [`PatchData`](PatchData#struct-patchdata).

**Functions**

- [`PatchData`](PatchData#struct-patchdata) ` Load(const std::string& filepath,` [`Status`](PatchData#enum-status)`* status = nullptr)`: Loads all patches from the given file. Optionally writes the result status to `status`.
- [`PatchData`](PatchData#struct-patchdata) ` Load(const std::string& filepath, uint32_t max_patches,` [`Status`](PatchData#enum-status)`* status = nullptr)`: Loads up to `max_patches` patches from the given file. Optionally writes the result status to `status`.