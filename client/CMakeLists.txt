# Client is Windows-only — Hogwarts Legacy does not run on other platforms.
add_library(HogwartsMPNet SHARED src/net_module.cpp)

target_link_libraries(HogwartsMPNet PRIVATE shared enet lua_static)
target_include_directories(HogwartsMPNet PRIVATE ${lua_SOURCE_DIR})

if(WIN32)
    target_link_libraries(HogwartsMPNet PRIVATE ws2_32 winmm)
    # Export as a Lua C extension — no lib prefix on Windows
    set_target_properties(HogwartsMPNet PROPERTIES PREFIX "")
endif()
