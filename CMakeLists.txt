cmake_minimum_required(VERSION 3.16)
project(local_mixing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(common_lib STATIC src/circuit.cpp src/adder.cpp src/sandwich.cpp)
target_include_directories(common_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Driver
add_executable(gate_driver src/main.cpp)
target_link_libraries(gate_driver PRIVATE common_lib)

# Tests
enable_testing()
add_executable(test_gates tests/test_gates.cpp)
target_link_libraries(test_gates PRIVATE common_lib)
add_test(NAME gates COMMAND test_gates)

add_executable(test_circuit tests/test_circuit.cpp)
target_link_libraries(test_circuit PRIVATE common_lib)
add_test(NAME circuit COMMAND test_circuit)

add_executable(test_sandwich tests/test_sandwich.cpp)
target_link_libraries(test_sandwich PRIVATE common_lib)
add_test(NAME sandwich COMMAND test_sandwich)
