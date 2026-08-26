cmake_minimum_required(VERSION 3.16)
project(local_mixing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only interface target so all executables share the same include path
add_library(gate_lib INTERFACE)
target_include_directories(gate_lib INTERFACE ${CMAKE_SOURCE_DIR}/include)

# Driver
add_executable(gate_driver src/main.cpp)
target_link_libraries(gate_driver PRIVATE gate_lib)

# Tests
enable_testing()
add_executable(test_gates tests/test_gates.cpp)
target_link_libraries(test_gates PRIVATE gate_lib)
add_test(NAME gates COMMAND test_gates)
