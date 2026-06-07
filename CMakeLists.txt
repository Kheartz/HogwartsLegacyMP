cmake_minimum_required(VERSION 3.28)
project(HogwartsLegacyMP C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.17
)
FetchContent_MakeAvailable(enet)

# ENet's old CMakeLists doesn't set include dirs on its target — patch it here
target_include_directories(enet PUBLIC ${enet_SOURCE_DIR}/include)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)

FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
)
FetchContent_MakeAvailable(Catch2 lua)

# Lua doesn't have a CMakeLists — build it as a static lib manually
file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${lua_SOURCE_DIR}/lua.c      # standalone interpreter
    ${lua_SOURCE_DIR}/luac.c     # compiler
    ${lua_SOURCE_DIR}/onelua.c   # unity build
)
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})

add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(tests)

if(WIN32)
    add_subdirectory(client)
endif()

enable_testing()
