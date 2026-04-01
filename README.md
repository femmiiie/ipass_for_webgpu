# iPASS For WebGPU

## Table of Contents
1. [Overview](#overview)
2. [Building](#building)
   - [Native](#native)
   - [Emscripten](#emscripten)
   - [Library](#library)
3. [Dependencies](#dependencies)
4. [License](#license)

# Overview

TODO

# Building

Build options are available for both native as well as a web app using Emscripten.
Clone the repository using `git clone https://github.com/femmiiie/senior_project.git`.

## Native 

**Prerequisites**
- **CMake** >= 3.5
- **C++23 compatible compiler**

```bash
cmake -B build
cmake --build build --config Release
```

The WebGPU backend can be specified with `-DBACKEND`. Available options are `DAWN` and `WGPU`.


For building tests:
`cmake --build . --target RUN_TESTS` or `ctest`

## Emscripten

**Prerequisites**
- **CMake** >= 3.5
- **C++23 compatible compiler**
- [**Emscripten SDK**](https://emscripten.org/docs/getting_started/downloads.html)
  - Ensure `emsdk` is in your PATH and initialized (the `emcmake` command should be working)

```bash
emcmake cmake -B build_wasm
cmake --build build_wasm --config Release
```

The output will be a `.html` file in `build_wasm/`. \
Run a web server (ex. `python -m http.server 8000`), and open `http://localhost:8000/ipass_webgpu.html` in your browser.


## Library

See the [library README](./library/README.md) for build steps and API reference.

# Dependencies

All dependencies are pulled automatically using CMake's FetchContent.
- [glfw](https://github.com/glfw/glfw)
- [glfw3webgpu](https://github.com/eliemichel/glfw3webgpu)
- [glm](https://github.com/g-truc/glm)
- [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended)
- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)
- [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution)

# License
This project is licensed under the (??? ask Dr. Peters about this).
PnS uses Creative Commons