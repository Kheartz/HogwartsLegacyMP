#include <catch2/catch_test_macros.hpp>
#include "Protocol.h"
#include <enet/enet.h>
#include <httplib.h>
#include <thread>
#include <chrono>

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

TEST_CASE("Warp: server delivers MsgWarp to target after HTTP POST", "[integration][!mayfail]")
{
    REQUIRE(enet_initialize() == 0);

    // Connect two clients
    auto make_client = []() -> ENetHost* {
        return enet_host_create(nullptr, 1, 2, 0, 0);
    };
    auto connect_peer = [](ENetHost* h) -> ENetPeer* {
        ENetAddress addr{};
        enet_address_set_host(&addr, "127.0.0.1");
        addr.port = 7777;
        return enet_host_connect(h, &addr, 2, 0);
    };

    ENetHost* ha = make_client();
    ENetHost* hb = make_client();
    REQUIRE(ha); REQUIRE(hb);

    ENetPeer* pa = connect_peer(ha);
    ENetPeer* pb = connect_peer(hb);
    REQUIRE(pa); REQUIRE(pb);

    // Wait for both to connect
    auto wait_connect = [](ENetHost* h) -> bool {
        ENetEvent ev{};
        return enet_host_service(h, &ev, 1000) > 0 && ev.type == ENET_EVENT_TYPE_CONNECT;
    };

    if (!wait_connect(ha) || !wait_connect(hb))
    {
        WARN("Server not reachable on 127.0.0.1:7777 — skipping warp integration test.");
        enet_peer_reset(pa); enet_host_destroy(ha);
        enet_peer_reset(pb); enet_host_destroy(hb);
        enet_deinitialize();
        return;
    }

    // Client A registers as player 1 at a known position
    const float src_x = 111111.0f, src_y = -222222.0f, src_z = -33333.0f;
    {
        MsgPlayerMove m{};
        m.header.opcode = Opcode::PlayerMove;
        m.player_id = 1;
        m.x = src_x; m.y = src_y; m.z = src_z; m.yaw = 45.0f;
        auto* pkt = enet_packet_create(&m, sizeof(m), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(pa, 0, pkt);
        enet_host_flush(ha);
    }

    // Client B registers as player 2
    {
        MsgPlayerMove m{};
        m.header.opcode = Opcode::PlayerMove;
        m.player_id = 2;
        m.x = 0; m.y = 0; m.z = 0; m.yaw = 0;
        auto* pkt = enet_packet_create(&m, sizeof(m), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(pb, 0, pkt);
        enet_host_flush(hb);
    }

    // Give the server time to register both players
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ENetEvent ev{};
    enet_host_service(ha, &ev, 0);
    enet_host_service(hb, &ev, 0);

    // POST /api/warp?target_id=2&source_id=1 via the dashboard HTTP server
    httplib::Client http("127.0.0.1", 8080);
    http.set_connection_timeout(1);
    auto res = http.Post("/api/warp?target_id=2&source_id=1");

    if (!res || res->status != 200)
    {
        WARN("Dashboard not reachable on port 8080 — skipping warp HTTP check.");
    }
    else
    {
        REQUIRE(res->body.find("true") != std::string::npos);
    }

    // Give ENet time to deliver the warp packet to client B
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Client B should receive a MsgWarp with player 1's position
    bool got_warp = false;
    while (enet_host_service(hb, &ev, 100) > 0)
    {
        if (ev.type == ENET_EVENT_TYPE_RECEIVE && ev.packet->dataLength >= sizeof(MsgWarp))
        {
            const auto* hdr = reinterpret_cast<const MsgHeader*>(ev.packet->data);
            if (hdr->opcode == Opcode::Warp)
            {
                const auto* warp = reinterpret_cast<const MsgWarp*>(ev.packet->data);
                REQUIRE(warp->x == src_x);
                REQUIRE(warp->y == src_y);
                REQUIRE(warp->z == src_z);
                got_warp = true;
            }
            enet_packet_destroy(ev.packet);
        }
    }
    REQUIRE(got_warp);

    enet_peer_disconnect(pa, 0); enet_host_service(ha, &ev, 300); enet_host_destroy(ha);
    enet_peer_disconnect(pb, 0); enet_host_service(hb, &ev, 300); enet_host_destroy(hb);
    enet_deinitialize();
}
