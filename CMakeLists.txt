cmake_minimum_required(VERSION 3.28)
project(HogwartsLegacyMP CXX)

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
FetchContent_MakeAvailable(Catch2)

add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(tests)

enable_testing()
