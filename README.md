# Snowflake Engine
Snowflake is a C++ Vulkan engine with the ability to render Signed Distance Fields (SDF) generated from triangle meshes.

# Getting Started
### Cloning The Repo
Clone the repo recursively to ensure all submodules are initialised:
```shell
git clone --recursive git@gitlab.com:WSWhitehouse/snowflake.git
```

If the repo wasn't cloned using `--recursive` use the following commands to update the submodules:
```shell
git submodule update --init
```

### Required Software
+ [Vulkan SDK v1.2.0+](https://vulkan.lunarg.com/sdk/home)
+ [CMake v3.22+](https://cmake.org/download/)
+ [LLVM / Clang Compiler v15.0.6](https://github.com/llvm/llvm-project/releases/tag/llvmorg-15.0.6)

*It is recommended to follow the versions above to maintain compatibility.*

### CMake 

1. Run the following command while in the root of the project to generate the build files. **REPLACE THE FILE PATHS!** Use `/` rather than `\` as CMake throws errors!
```shell
cmake  -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM="PATH_TO_NINJA/ninja.exe" -DCMAKE_C_COMPILER="PATH_TO_CLANG/clang.exe" -DCMAKE_CXX_COMPILER="PATH_TO_CLANG++/clang++.exe"
```

2. Run this command to build the project and it's relevant targets:
```shell
cmake --build ./cmake-build --target data shaders snowflake
```

3. Run the application (or double click the `exe`):
```shell
cd cmake-build & snowflake.exe
```

#### Targets
+ `data` - Sets up the files (meshes, textures, etc.) in the data directory and moves them to the output directory.
+ `shaders` - Compiles all GLSL shaders in the shaders directory and moves them to the output directory.
+ `snowflake` - Builds the engine executable.

*The data and shaders targets only need to be run when new data files or shaders are added. But are **required to be run once
before launching the executable** to ensure the data is initialised and all shaders have been compiled. If these targets haven't 
been run, the engine will crash as it won't be able to find the relative files.*

