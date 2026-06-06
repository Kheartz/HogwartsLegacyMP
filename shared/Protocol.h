#pragma once
#include <cstdint>

// All structs are packed to avoid platform padding differences across client/server.
#pragma pack(push, 1)

enum class Opcode : uint8_t
{
    Connect    = 0x01,
    Disconnect = 0x02,
    Heartbeat  = 0x03,
    PlayerMove = 0x10,
};

struct MsgHeader
{
    Opcode opcode;
};

struct MsgPlayerMove
{
    MsgHeader header;
    uint32_t  player_id;
    float     x, y, z;
    float     yaw;
};

struct MsgHeartbeat
{
    MsgHeader header;
};

#pragma pack(pop)
