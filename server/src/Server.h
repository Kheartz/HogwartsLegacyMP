#pragma once
#include <enet/enet.h>
#include <cstdint>

class Server
{
public:
    bool init(uint16_t port, int max_clients = 32);
    void run();
    void shutdown();

private:
    void on_connect(ENetPeer* peer);
    void on_disconnect(ENetPeer* peer);
    void on_packet(ENetPeer* peer, ENetPacket* packet);

    ENetHost* m_host = nullptr;
};
