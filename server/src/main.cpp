#include "Server.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[])
{
    uint16_t port = 7777;
    if (argc > 1)
        port = static_cast<uint16_t>(std::atoi(argv[1]));

    printf("=== HogwartsLegacyMP Server ===\n");

    Server server;
    if (!server.init(port))
        return 1;

    server.run();
    server.shutdown();
    return 0;
}
