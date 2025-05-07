cmake_minimum_required(VERSION 3.1...3.28)

project(bruteforcer VERSION 1.0 LANGUAGES CXX)
find_package(OpenMP REQUIRED)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(bruteforcer src/main.cpp)
target_link_libraries(bruteforcer PRIVATE OpenMP::OpenMP_CXX)
install(TARGETS bruteforcer DESTINATION .)

if (UNIX)
target_compile_options(bruteforcer PRIVATE -Wall)
endif()

if(WIN32)
target_compile_options(bruteforcer PRIVATE /W4)
endif()
