cmake_minimum_required(VERSION 3.16)
project(local_mixing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(common_lib STATIC
    src/circuit.cpp
    src/adder.cpp
    src/sandwich.cpp
    src/shuffle.cpp
    src/inject_junk.cpp
    src/pipeline.cpp
    src/local_replace.cpp
)
target_include_directories(common_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Driver
add_executable(gate_driver src/main.cpp)
target_link_libraries(gate_driver PRIVATE common_lib)

# ADR 0007 CLI-level checks (ticket 0030): gate_driver exit codes.
add_test(NAME cli_key_empty_errors COMMAND gate_driver --key=)
set_tests_properties(cli_key_empty_errors PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_key_then_seed COMMAND gate_driver --key=demo --seed=7 --no-inject)
add_test(NAME cli_seed_only COMMAND gate_driver --seed=7 --no-inject)

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

add_executable(test_shuffle tests/test_shuffle.cpp)
target_link_libraries(test_shuffle PRIVATE common_lib)
add_test(NAME shuffle COMMAND test_shuffle)

add_executable(test_inject_junk tests/test_inject_junk.cpp)
target_link_libraries(test_inject_junk PRIVATE common_lib)
add_test(NAME inject_junk COMMAND test_inject_junk)

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE common_lib)
add_test(NAME pipeline COMMAND test_pipeline)

add_executable(test_adder tests/test_adder.cpp)
target_link_libraries(test_adder PRIVATE common_lib)
add_test(NAME adder COMMAND test_adder)

add_executable(test_local_replace tests/test_local_replace.cpp)
target_link_libraries(test_local_replace PRIVATE common_lib)
add_test(NAME local_replace COMMAND test_local_replace)
