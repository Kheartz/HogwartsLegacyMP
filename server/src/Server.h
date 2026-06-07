#pragma once
#include <enet/enet.h>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

struct PlayerState {
    uint32_t id  = 0;
    float    x   = 0, y = 0, z = 0, yaw = 0;
    std::chrono::steady_clock::time_point last_seen{};
};

struct WarpCommand {
    uint32_t target_id;
    float    x, y, z;
};

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
    void send_warp(uint32_t target_id, float x, float y, float z);
    void start_http(uint16_t http_port);

    ENetHost* m_host = nullptr;

    // Player state — shared with HTTP thread, protected by m_mutex
    std::mutex                                m_mutex;
    std::unordered_map<uint32_t, PlayerState> m_players;

    // Peer map — ENet thread only, no mutex needed
    std::unordered_map<uint32_t, ENetPeer*>   m_peers;

    // Warp command queue — written by HTTP thread, drained by ENet thread
    std::mutex                 m_warp_mutex;
    std::queue<WarpCommand>    m_warp_queue;

    std::thread m_http_thread;
};
