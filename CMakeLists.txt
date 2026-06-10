cmake_minimum_required(VERSION 3.22)
project(voxfall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_library(voxfall_core
    src/world/world.cpp
    src/world/mapfile.cpp
    src/vehicle/vehicle.cpp
    src/vehicle/templates.cpp
    src/vehicle/locomotion.cpp
    src/sim/sim.cpp
    src/net/net.cpp
    src/render/mesh.cpp
    src/ai/orders.cpp
    src/core/png.cpp
    src/render/raycast.cpp
)
target_include_directories(voxfall_core PUBLIC src)

add_executable(voxfall_sim src/sim/main.cpp)
target_link_libraries(voxfall_sim PRIVATE voxfall_core)

add_executable(voxfall_preview src/tools/preview.cpp)
target_link_libraries(voxfall_preview PRIVATE voxfall_core)

enable_testing()
add_executable(voxfall_tests
    tests/test_main.cpp
    tests/test_world.cpp
    tests/test_vehicle.cpp
    tests/test_locomotion.cpp
    tests/test_mapfile.cpp
    tests/test_sim.cpp
    tests/test_net.cpp
    tests/test_mesh.cpp
    tests/test_ai.cpp
    tests/test_preview.cpp
)
target_link_libraries(voxfall_tests PRIVATE voxfall_core)
add_test(NAME voxfall_unit COMMAND voxfall_tests)
