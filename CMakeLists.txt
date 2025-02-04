cmake_minimum_required(VERSION 3.25)

include(fetchContent)

# this has to be set before project()
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES native)
endif()

project(test LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CUDA_STANDARD 23)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
set(FETCHCONTENT_QUIET FALSE)
set(GIT_PROGRESS TRUE)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/bin)

# CPM
file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)



set(LIBS)

add_executable(main src/main.cpp)

# Include directories
target_include_directories(main
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

# CUDA
add_library(qda 
    src/kernel.cu
    "include/kernel.cuh"
)
set_target_properties(qda PROPERTIES LANGUAGE CUDA)
target_include_directories(qda 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
target_link_libraries(main PRIVATE qda)

# SDL3
CPMAddPackage("gh:libsdl-org/SDL#2fa1e72")
list(APPEND LIBS SDL3::SDL3)

# SDL_image
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE) # Requires Perl and Nasm for libaom (AVIF image format)
CPMAddPackage("gh:libsdl-org/SDL_image#4a762bd")
list(APPEND LIBS SDL3_image::SDL3_image)

# glm
CPMAddPackage("gh:g-truc/glm#3c18b0f")
list(APPEND LIBS glm::glm-header-only)

# assimp
CPMAddPackage("gh:assimp/assimp#5fa7b8c")
list(APPEND LIBS assimp::assimp)

target_link_libraries(main PRIVATE ${LIBS})
target_compile_features(main PRIVATE cxx_std_23)

# Copy Assets
file(COPY Content DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)