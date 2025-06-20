cmake_minimum_required(VERSION 3.1...3.28)

# embed kernels
file(READ "src/cl/hashlittle2.cl" KERNEL_HASHLITTLE2)
configure_file("src/cl/kernels.h" "src/kernels.h" @ONLY)

project(bruteforcer VERSION 1.0 LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenCL REQUIRED)
add_executable(bruteforcer src/main.cpp)
target_compile_definitions(bruteforcer PRIVATE CL_TARGET_OPENCL_VERSION=300)
target_sources(bruteforcer PRIVATE src/cl/errors.cpp)
target_sources(bruteforcer PRIVATE src/gpu.cpp)
target_sources(bruteforcer PRIVATE src/main.cpp)
target_sources(bruteforcer PRIVATE src/util.cpp)
target_link_libraries(bruteforcer OpenCL::OpenCL)
install(TARGETS bruteforcer DESTINATION .)

if (UNIX)
target_compile_options(bruteforcer PRIVATE -Wall)
endif()

if (WIN32)
target_compile_options(bruteforcer PRIVATE /W4 /EHsc)
endif()
