
# BRDF Viewer

A real-time BRDF (Bidirectional Reflectance Distribution Function) visualization tool built using Vulkan & GLFW.

### Features

* Real-time rendering
* Multiple BRDFs
    * Lambertian, Oren-Nayar, Burley, Blinn-Phong, Ward, Cook-Torrance(GGX), Disney Principled BRDF
* Image based lighting (HDR)
* Interactive camera
* Wavefront objects supported 
* GUI settings

### Requirements

* Vulkan SDK
* CMake
* GLFW
* GLM
* Vulkan-compatible GPU

### Build

```bash
git clone https://github.com/orsrozsondai/brdf-viewer.git
cd brdf-viewer
mkdir build && cd build
cmake .. # -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Run

```bash
./build/bin/brdf-viewer # from the project directory
```

### Controls
| Action | Command |
| ----------- | ----------- |
| RMB + Move | Move camera |
| Scroll | Zoom |
| CTRL + Scroll, Arrows(L,R) | Select next/previous object |
| CTRL + M | Show/hide settings |
