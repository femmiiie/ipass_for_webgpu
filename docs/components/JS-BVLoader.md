# BV Loader (JavaScript)

Utility for loading patch data from `.bv` files in JavaScript. Mirrors [`ipass::LoadBV`](BV-Loader) in C++.

## Public Functions

### `function loadBVFile`

Exposed on the loaded module as `Module.loadBVFile`.

**Functions**

- [`BVLoadResult`](JS-PatchData#interface-bvloadresult) `loadBVFile(data: ArrayBuffer, options?: { maxPatches?: number })`: Parses a `.bv` file already read into an `ArrayBuffer`, up to `options.maxPatches` patches (`0` or omitted = no limit).
