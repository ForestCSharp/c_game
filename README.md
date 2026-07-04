# c_game

`c_game` is an exercise in minimalism: a personal challenge to build a renderer
and game runtime in C with as few external libraries as possible.

The project depends on the C standard library, platform APIs, shader
tools, and whichever graphics stack is selected for the build. It is still a
research-and-learning workspace rather than a polished engine distribution, but
it has become a useful place to experiment with rendering, animation, game
objects, and physics from first principles.

A great idea or an exercise in futility, you decide.

## Current Capabilities

The engine includes:

- A C11 runtime with platform-specific window and input layers for macOS and
  Windows.
- GPU abstraction work with Vulkan and Metal implementations.
- macOS backend selection between Metal, Vulkan over MoltenVK, and a KosmicKrisp
  Vulkan path.
- GLSL shader compilation to SPIR-V and Metal Shading Language outputs.
- Static and animated glTF model loading.
- Game object/component storage for transforms, models, render data, player
  controls, and cameras.
- Debug drawing for spheres, boxes, meshes, solid shapes, and wireframes.
- An immediate-mode GUI system with bitmap-font rendering, buttons, sliders,
  windows, scrolling, and resizing.
- Task/threading utilities with platform-specific threading support.
- Experimental rigid-body physics, broad phase collision detection, GJK/EPA
  collision work, constraints, and contact resolution.

## How It Works

The basic development loop is:

```text
source files + data assets
  -> compile_shaders.sh builds bin/shaders/
  -> build.sh selects a GPU backend and compiles src/main.c
  -> platform window/app layer creates a native window
  -> game loop updates input, animation, physics, rendering, debug draw, and GUI
```

The project intentionally keeps most systems close to the source. `src/main.c`
includes the selected GPU implementation, creates the demo scene, loads assets
from `data/`, updates game objects and physics, and records render passes for
the main scene, debug draw, and GUI.

## Project Layout

- `src/`: engine and game source, including app, GPU, math, model, physics,
  memory, threading, GUI, debug draw, and game object code.
- `src/app/`: platform window/input implementations for macOS and Win32.
- `src/gpu/`: backend-neutral GPU interface plus Vulkan and Metal
  implementations.
- `src/model/` and `src/gltf.h`: static and animated glTF loading.
- `src/physics/`: collision, rigid-body, constraint, and physics debug-rendering
  experiments.
- `data/shaders/`: GLSL shaders compiled by `compile_shaders.sh`.
- `data/meshes/`: sample `.glb` assets used by the demo scene.
- `data/fonts/`: bitmap and TrueType font assets used by GUI/font experiments.
- `build.sh`, `build.bat`: main build-and-run entrypoints.
- `compile_shaders.sh`: shader-only build helper.
- `gpu_test.sh`, `test.sh`: smoke-test entrypoints.
- `capture.bat`: RenderDoc capture helper for the Windows executable.
- `docs/`: documentation directory; it has no substantive project docs yet.

## Platform Notes

- macOS is the primary development path.
- Windows is supported through `build.bat`, which calls `build.sh` from a shell
  environment with `bash.exe`.
- Linux is detected by the scripts, but `build.sh` and `gpu_test.sh` do not
  implement a Linux build branch.
- The default macOS path uses Vulkan over MoltenVK.
- The Metal backend is available through the macOS build script option.

## Prerequisites

Common requirements:

- `bash`
- `clang`
- C11 compiler support
- `glslc`
- `spirv-cross`

Vulkan builds also require:

- Vulkan headers and libraries.
- On macOS, `/usr/local/lib/libvulkan.dylib` for the existing scripts.
- On Windows, `VULKAN_SDK` with `Include` and `Lib/vulkan-1.lib`.

macOS builds link against system frameworks:

- Cocoa
- Metal
- MetalKit
- Quartz

Optional tools:

- RenderDoc, if using `capture.bat`.

## Build And Run

Run commands from the repository root.

### Default macOS Vulkan/MoltenVK Path

```sh
./build.sh
```

This clears and recreates `bin/`, compiles shaders from `data/shaders`, builds
`src/main.c` with the Vulkan GPU implementation and MoltenVK portability
extensions, copies `/usr/local/lib/libvulkan.dylib` into `bin/`, and runs
`bin/game`.

### Metal Backend

```sh
./build.sh -mb metal
./build.sh --mac_backend metal
```

Either form builds and runs the game with the Metal GPU implementation.

### Vulkan Over MoltenVK

```sh
./build.sh -mb vk_molten
```

This is the default macOS Vulkan path. It defines the Vulkan GPU implementation,
the MoltenVK macOS backend, and Vulkan portability extensions.

### KosmicKrisp Vulkan Backend

```sh
./build.sh -mb vk_kosmic
```

This selects the Vulkan GPU implementation with the KosmicKrisp macOS backend.
The script expects the ICD file at:

```text
/usr/local/share/vulkan/icd.d/libkosmickrisp_icd.json
```

### Windows

```bat
build.bat
```

`build.bat` calls `bash.exe build.sh`. On MinGW-style environments, `build.sh`
compiles `src/main.c` to `bin/game.exe`, links against `user32.lib` and
`${VULKAN_SDK}/Lib/vulkan-1.lib`, and runs the executable.

## Testing And Smoke Tests

### General C Test

```sh
./test.sh
```

This builds `src/test.c` into `bin/test` and runs it.

### GPU Test

```sh
./gpu_test.sh
./gpu_test.sh metal
./gpu_test.sh mtl
./gpu_test.sh vulkan
./gpu_test.sh vk
```

The GPU test script rebuilds shaders, compiles `src/gpu/gpu_test.c`, and runs
the resulting executable. If no backend is specified, it defaults to Vulkan.

### Shader-Only Compilation

```sh
./compile_shaders.sh data/shaders
```

This writes SPIR-V and Metal Shading Language outputs under `bin/shaders/`.

## Development Notes

- `bin/` is generated by the build and test scripts and is ignored by git.
- Shader compilation emits `.spv` files and `.msl` files into `bin/shaders/`.
- `build.sh` enables several Metal and MoltenVK debug environment variables by
  default.
- The main game executable expects to be run from the repository root so it can
  load assets through paths such as `data/meshes/...` and `data/fonts/...`.
- The codebase is intentionally experimental. Prefer small, understandable
  changes that preserve the minimal-dependency spirit of the project.
