add_executable(HogwartsTests
    test_protocol.cpp
    test_client.cpp
)

target_link_libraries(HogwartsTests PRIVATE
    shared
    enet
    Catch2::Catch2WithMain
)

if(WIN32)
    target_link_libraries(HogwartsTests PRIVATE ws2_32 winmm)
endif()

include(CTest)
include(Catch)
catch_discover_tests(HogwartsTests)
