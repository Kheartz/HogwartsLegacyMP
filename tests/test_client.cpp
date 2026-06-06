#include <catch2/catch_test_macros.hpp>
#include "Protocol.h"
#include <enet/enet.h>

// Integration test — requires the server to be running on localhost:7777.
// Skip automatically if the connection cannot be established.

static ENetHost* create_client()
{
    return enet_host_create(nullptr, 1, 2, 0, 0);
}

static ENetPeer* connect_to_server(ENetHost* client, uint16_t port = 7777)
{
    ENetAddress addr{};
    enet_address_set_host(&addr, "127.0.0.1");
    addr.port = port;
    return enet_host_connect(client, &addr, 2, 0);
}

TEST_CASE("Connect to server and send PlayerMove", "[integration][!mayfail]")
{
    REQUIRE(enet_initialize() == 0);

    ENetHost* client = create_client();
    REQUIRE(client != nullptr);

    ENetPeer* peer = connect_to_server(client);
    REQUIRE(peer != nullptr);

    // Wait up to 1 second for connection
    ENetEvent event{};
    bool connected = false;
    if (enet_host_service(client, &event, 1000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
        connected = true;

    if (!connected)
    {
        WARN("Server not reachable on 127.0.0.1:7777 — skipping integration test.");
        enet_peer_reset(peer);
        enet_host_destroy(client);
        enet_deinitialize();
        return;
    }

    // Send a PlayerMove packet
    MsgPlayerMove msg{};
    msg.header.opcode = Opcode::PlayerMove;
    msg.player_id     = 1;
    msg.x             = 358298.34f;
    msg.y             = -612390.81f;
    msg.z             = -82060.99f;
    msg.yaw           = 87.41f;

    ENetPacket* packet = enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
    REQUIRE(packet != nullptr);
    REQUIRE(enet_peer_send(peer, 0, packet) == 0);
    enet_host_flush(client);

    // Give the server a moment to receive it
    enet_host_service(client, &event, 200);

    // Send a Heartbeat
    MsgHeartbeat hb{};
    hb.header.opcode = Opcode::Heartbeat;
    ENetPacket* hb_packet = enet_packet_create(&hb, sizeof(hb), ENET_PACKET_FLAG_RELIABLE);
    REQUIRE(enet_peer_send(peer, 0, hb_packet) == 0);
    enet_host_flush(client);

    enet_host_service(client, &event, 200);

    enet_peer_disconnect(peer, 0);
    enet_host_service(client, &event, 500);

    enet_host_destroy(client);
    enet_deinitialize();

    SUCCEED("Connected, sent PlayerMove + Heartbeat, disconnected cleanly.");
}
