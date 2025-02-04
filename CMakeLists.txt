cmake_minimum_required(VERSION 3.25)

include(fetchContent)

# this has to be set before project()
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES native)
endif()

project(test LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CUDA_STANDARD 26)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/bin)

add_executable(main src/main.cpp)

# Include directories
target_include_directories(main
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

# CUDA
add_library(qda 
    src/kernel.cu
    "include/kernel.cuh"
)
set_target_properties(qda PROPERTIES LANGUAGE CUDA)
target_link_libraries(main PRIVATE qda)

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.2
)
FetchContent_MakeAvailable(SDL3)
target_link_libraries(main PRIVATE SDL3::SDL3)

# # SDL_shadercross *(not working)*
# FetchContent_Declare(
#     SDL_shadercross
#     GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
#     GIT_TAG 
#)
# FetchContent_MakeAvailable(SDL_shadercross)
# target_link_libraries(main PRIVATE SDL_shadercross)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
target_link_libraries(main PRIVATE glm::glm)

# Copy Assets
file(COPY Content DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)