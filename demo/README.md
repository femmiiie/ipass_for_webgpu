# iPASS For WebGPU Demo

## Table of Contents

1. [Overview](#overview)
2. [Building](#building)
   - [Native](#native)
   - [Emscripten](#emscripten)
3. [Dependencies](#dependencies)

## Overview

This is a companion demo app to the iPASS for WebGPU library, showing its capabilities, performance, as well as serving as a usage example of how the library would be used in real world applications.

## Building

The app is able to be built for native as well as a web app using Emscripten.\
Clone the repository using `git clone https://github.com/femmiiie/ipass_for_webgpu.git`.

### Native

#### Prerequisites

- **CMake** >= 3.5
- **C++23 compatible compiler**

```bash
cmake -B build -S ./demo
cmake --build build --config Release
```

The WebGPU backend can be specified with `-DBACKEND`. Available options are `DAWN` and `WGPU`.

### Emscripten

#### Prerequisites

- **CMake** >= 3.5
- **C++23 compatible compiler**
- [**Emscripten SDK**](https://emscripten.org/docs/getting_started/downloads.html)
  - Ensure `emsdk` is in your PATH and initialized (the `emcmake` command should be working)

```bash
emcmake cmake -B build_wasm -S ./demo
cmake --build build_wasm --config Release
```

The output will be a `.html` file in `build_wasm/`. \
Run a web server (e.g. `python -m http.server 8000`), and open `http://localhost:8000/ipass_webgpu.html` in your browser.

## Dependencies

All dependencies are pulled automatically using CMake's FetchContent.

- [glfw](https://github.com/glfw/glfw)
- [glfw3webgpu](https://github.com/eliemichel/glfw3webgpu)
- [glm](https://github.com/g-truc/glm)
- [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended)
- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)
- [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution)
