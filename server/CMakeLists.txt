add_executable(HogwartsServer
    src/main.cpp
    src/Server.cpp
    src/Server.h
)

find_package(Threads REQUIRED)

target_link_libraries(HogwartsServer PRIVATE shared enet httplib::httplib Threads::Threads)
target_include_directories(HogwartsServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(WIN32)
    target_link_libraries(HogwartsServer PRIVATE ws2_32 winmm)
endif()
