#include "Protocol.h"
#include <enet/enet.h>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <cstring>
#include <cstdio>

// Single global ENet state — one connection at a time.
static ENetHost* g_host = nullptr;
static ENetPeer* g_peer = nullptr;
static bool      g_initialized = false;

static void cleanup()
{
    if (g_peer)  { enet_peer_reset(g_peer);    g_peer = nullptr; }
    if (g_host)  { enet_host_destroy(g_host);  g_host = nullptr; }
    if (g_initialized) { enet_deinitialize();  g_initialized = false; }
}

// net.connect(ip, port) -> true | nil, error
static int l_connect(lua_State* L)
{
    const char* ip   = luaL_checkstring(L, 1);
    int         port = static_cast<int>(luaL_checkinteger(L, 2));

    cleanup();

    if (enet_initialize() != 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "enet_initialize failed");
        return 2;
    }
    g_initialized = true;

    g_host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!g_host)
    {
        cleanup();
        lua_pushnil(L);
        lua_pushstring(L, "enet_host_create failed");
        return 2;
    }

    ENetAddress addr{};
    enet_address_set_host(&addr, ip);
    addr.port = static_cast<enet_uint16>(port);

    g_peer = enet_host_connect(g_host, &addr, 2, 0);
    if (!g_peer)
    {
        cleanup();
        lua_pushnil(L);
        lua_pushstring(L, "enet_host_connect failed");
        return 2;
    }

    // Wait up to 2 seconds for the connection handshake
    ENetEvent event{};
    if (enet_host_service(g_host, &event, 2000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        printf("[HogwartsMPNet] Connected to %s:%d\n", ip, port);
        lua_pushboolean(L, 1);
        return 1;
    }

    enet_peer_reset(g_peer);
    g_peer = nullptr;
    lua_pushnil(L);
    lua_pushstring(L, "connection timed out");
    return 2;
}

// net.send_position(player_id, x, y, z, yaw)
static int l_send_position(lua_State* L)
{
    if (!g_peer)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    auto player_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto x   = static_cast<float>(luaL_checknumber(L, 2));
    auto y   = static_cast<float>(luaL_checknumber(L, 3));
    auto z   = static_cast<float>(luaL_checknumber(L, 4));
    auto yaw = static_cast<float>(luaL_checknumber(L, 5));

    MsgPlayerMove msg{};
    msg.header.opcode = Opcode::PlayerMove;
    msg.player_id     = player_id;
    msg.x = x; msg.y = y; msg.z = z; msg.yaw = yaw;

    ENetPacket* pkt = enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_UNSEQUENCED);
    if (enet_peer_send(g_peer, 0, pkt) != 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "enet_peer_send failed");
        return 2;
    }

    enet_host_flush(g_host);
    lua_pushboolean(L, 1);
    return 1;
}

// net.disconnect()
static int l_disconnect(lua_State* L)
{
    cleanup();
    lua_pushboolean(L, 1);
    return 1;
}

// net.poll() -> array of {player_id, x, y, z, yaw} tables for received PlayerMove packets
static int l_poll(lua_State* L)
{
    lua_newtable(L);
    if (!g_host) return 1;

    int count = 0;
    ENetEvent event{};
    while (enet_host_service(g_host, &event, 0) > 0)
    {
        if (event.type == ENET_EVENT_TYPE_DISCONNECT)
        {
            printf("[HogwartsMPNet] Disconnected from server.\n");
            g_peer = nullptr;
        }
        else if (event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            auto* data = event.packet->data;
            auto  len  = event.packet->dataLength;
            if (len >= sizeof(MsgHeader))
            {
                const auto* hdr = reinterpret_cast<const MsgHeader*>(data);
                if (hdr->opcode == Opcode::PlayerMove && len >= sizeof(MsgPlayerMove))
                {
                    const auto* msg = reinterpret_cast<const MsgPlayerMove*>(data);
                    lua_newtable(L);
                    lua_pushinteger(L, msg->player_id); lua_setfield(L, -2, "player_id");
                    lua_pushnumber(L,  msg->x);         lua_setfield(L, -2, "x");
                    lua_pushnumber(L,  msg->y);         lua_setfield(L, -2, "y");
                    lua_pushnumber(L,  msg->z);         lua_setfield(L, -2, "z");
                    lua_pushnumber(L,  msg->yaw);       lua_setfield(L, -2, "yaw");
                    lua_rawseti(L, -2, ++count);
                }
            }
            enet_packet_destroy(event.packet);
        }
    }
    return 1;
}

static const luaL_Reg net_funcs[] = {
    { "connect",       l_connect       },
    { "send_position", l_send_position },
    { "disconnect",    l_disconnect    },
    { "poll",          l_poll          },
    { nullptr, nullptr }
};

#if defined(_WIN32)
#  define MOD_EXPORT extern "C" __declspec(dllexport)
#else
#  define MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

MOD_EXPORT int luaopen_HogwartsMPNet(lua_State* L)
{
    luaL_newlib(L, net_funcs);
    return 1;
}
