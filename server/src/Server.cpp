#include "Server.h"
#include "Protocol.h"
#include <cstdio>
#include <cstring>

bool Server::init(uint16_t port, int max_clients)
{
    if (enet_initialize() != 0)
    {
        printf("[server] Failed to initialize ENet.\n");
        return false;
    }

    ENetAddress addr{};
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    m_host = enet_host_create(&addr, max_clients, 2, 0, 0);
    if (!m_host)
    {
        printf("[server] Failed to create ENet host on port %u.\n", port);
        return false;
    }

    printf("[server] Listening on port %u (max %d clients).\n", port, max_clients);
    return true;
}

void Server::run()
{
    ENetEvent event{};
    while (true)
    {
        while (enet_host_service(m_host, &event, 16) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                on_connect(event.peer);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                on_packet(event.peer, event.packet);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                on_disconnect(event.peer);
                break;
            default:
                break;
            }
        }
    }
}

void Server::shutdown()
{
    if (m_host)
    {
        enet_host_destroy(m_host);
        m_host = nullptr;
    }
    enet_deinitialize();
}

void Server::on_connect(ENetPeer* peer)
{
    printf("[server] Client connected: %u:%u\n", peer->address.host, peer->address.port);
    peer->data = nullptr;
}

void Server::on_disconnect(ENetPeer* peer)
{
    printf("[server] Client disconnected: %u:%u\n", peer->address.host, peer->address.port);
}

void Server::on_packet(ENetPeer* peer, ENetPacket* packet)
{
    if (packet->dataLength < sizeof(MsgHeader))
        return;

    const auto* header = reinterpret_cast<const MsgHeader*>(packet->data);

    switch (header->opcode)
    {
    case Opcode::Heartbeat:
        printf("[server] Heartbeat from %u:%u\n", peer->address.host, peer->address.port);
        break;

    case Opcode::PlayerMove:
        if (packet->dataLength >= sizeof(MsgPlayerMove))
        {
            const auto* msg = reinterpret_cast<const MsgPlayerMove*>(packet->data);
            printf("[server] PlayerMove id=%u  X=%.1f Y=%.1f Z=%.1f Yaw=%.1f\n",
                msg->player_id, msg->x, msg->y, msg->z, msg->yaw);
        }
        break;

    default:
        printf("[server] Unknown opcode 0x%02x\n", static_cast<uint8_t>(header->opcode));
        break;
    }
}
