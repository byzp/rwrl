cmake_minimum_required(VERSION 3.20)
project(rwrl_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)

add_compile_options(-Wno-attributes)
add_compile_definitions($<$<COMPILE_LANGUAGE:CXX>:_GNU_SOURCE>)

set(RWRL_SOURCES
    src/unit.cpp
    src/registry/builtins.cpp
    src/registry/lookup.cpp
    src/spatial.cpp
    src/lifecycle/world.cpp
    src/lifecycle/production.cpp
    src/lifecycle/construction.cpp
    src/world_visibility.cpp
    src/combat/weapons.cpp
    src/combat/projectiles.cpp
    src/combat/targeting.cpp
    src/navigation/movement/orders/navigation.cpp
    src/navigation/movement/runtime/api.cpp
    src/navigation/movement/runtime/update.cpp
    src/navigation/path.cpp
    src/navigation/behaviors/basic.cpp
    src/navigation/behaviors/service.cpp
    src/navigation/behaviors/commands.cpp
    src/navigation/behaviors/transport.cpp
    src/navigation/behaviors/flight.cpp
    src/navigation/behaviors/legs.cpp
    src/navigation/behaviors/submersible.cpp
    src/orders/lifecycle/activation.cpp
    src/orders/lifecycle/submission.cpp
    src/orders/lifecycle/completion.cpp
    src/orders/commands/api.cpp
    src/orders/commands/formation.cpp
    src/orders/commands/dispatch.cpp
    src/custom/actions/resources.cpp
    src/custom/actions/unit_effects.cpp
    src/custom/actions/execution.cpp
    src/world_unit_conversion.cpp
    src/systems/runtime.cpp
    src/systems/defence_production.cpp
    src/systems/environment.cpp
    src/systems/collision.cpp
    src/world_step.cpp
    src/engine.cpp
    src/navigation/pathfinding/grid.cpp
    src/navigation/pathfinding/search.cpp
    src/map_loader.cpp
)

add_library(rwrl_engine STATIC ${RWRL_SOURCES})
target_include_directories(rwrl_engine PUBLIC include)
target_include_directories(rwrl_engine PRIVATE src)

find_package(EXPAT REQUIRED)
find_package(ZLIB REQUIRED)
target_link_libraries(rwrl_engine PUBLIC EXPAT::EXPAT ZLIB::ZLIB)

find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(rwrl_core pybind/bind.cpp)
    target_link_libraries(rwrl_core PRIVATE rwrl_engine)
    target_include_directories(rwrl_core PRIVATE include)
    message(STATUS "pybind11 found — Python bindings enabled")
else()
    message(STATUS "pybind11 not found — Python bindings disabled (install: pip install pybind11)")
endif()
