# C-Common
A collection of useful files to include in C/C++ programs.

## What is C-Common
As the tagline above suggests - its a collection of files that contain commonly used functionality written in C! Check out the [docs](https://wswhitehouse.gitlab.io/c-common) for more information.

## How to build
- Add this repository as a submodule.
- Add `c-common/include` directory as an Include path.
- Compile all source files in the `c-common/src` directory as a part of your program.

## How to use in a C/C++ program
Include `common.h` header file at the top of all header and source files that require the common library. The `common.h` file includes all necessary files needed to use all functionality of c-common, the library is wrapped in `extern "C"` to ensure it builds correctly with a C++ compiler.
