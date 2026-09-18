# BV Loader

Utility for loading patch data from `.bv` files.

## Public Functions

### `namespace ipass`

Free function for parsing `.bv` files into [`PatchData`](PatchData#struct-patchdata).

**Functions**

- [`PatchData`](PatchData#struct-patchdata) ` LoadBV(const std::string& filepath,` [`Status`](PatchData#enum-status)`* status = nullptr, uint32_t max_patches = 0)`: Loads patches from the given file, up to `max_patches` (0 = no limit). Optionally writes the result status to `status`.
