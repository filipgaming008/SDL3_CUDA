# SDL_CUDA C++ Project

## Overview

This project combines SDL3 and CUDA to create a particle physics simulator. SDL3 is used for rendering graphics and handling input, while CUDA accelerates the computation of particle interactions on the GPU, providing high performance for complex simulations.

**Thanks to Codotaku and TheSpydog, most of this code for SDL3 GPU is from them!**

- **Codotaku - http://codotaku.com**
- **TheSpyDog - https://github.com/TheSpydog**

## Requirements

- CUDA Toolkit
- MSVC C++ Compiler
- CMake
- Requires Contents folder from: https://github.com/TheSpydog/SDL_gpu_examples

## Building

1. Clone the repository.
2. Navigate to the project directory.
3. Run `cmake -S . -B build` to generate the build files.
4. Run `cmake --build build` to build the project.

## Running

Execute the compiled binary to run the application.