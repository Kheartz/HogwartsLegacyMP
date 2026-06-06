#include <catch2/catch_test_macros.hpp>
#include "Protocol.h"
#include <cstdint>
#include <cstring>

TEST_CASE("MsgHeader size and opcode values", "[protocol]")
{
    REQUIRE(sizeof(MsgHeader) == 1);

    REQUIRE(static_cast<uint8_t>(Opcode::Connect)    == 0x01);
    REQUIRE(static_cast<uint8_t>(Opcode::Disconnect) == 0x02);
    REQUIRE(static_cast<uint8_t>(Opcode::Heartbeat)  == 0x03);
    REQUIRE(static_cast<uint8_t>(Opcode::PlayerMove) == 0x10);
}

TEST_CASE("MsgPlayerMove is packed with no padding", "[protocol]")
{
    // opcode(1) + player_id(4) + x(4) + y(4) + z(4) + yaw(4) = 21 bytes
    REQUIRE(sizeof(MsgPlayerMove) == 21);
}

TEST_CASE("MsgPlayerMove serializes and deserializes correctly", "[protocol]")
{
    MsgPlayerMove msg{};
    msg.header.opcode = Opcode::PlayerMove;
    msg.player_id     = 42;
    msg.x             = 358298.34f;
    msg.y             = -612390.81f;
    msg.z             = -82060.99f;
    msg.yaw           = 87.41f;

    // Round-trip via raw bytes (simulates what goes over the wire)
    uint8_t buf[sizeof(MsgPlayerMove)];
    std::memcpy(buf, &msg, sizeof(msg));

    MsgPlayerMove out{};
    std::memcpy(&out, buf, sizeof(out));

    REQUIRE(out.header.opcode == Opcode::PlayerMove);
    REQUIRE(out.player_id     == 42);
    REQUIRE(out.x             == msg.x);
    REQUIRE(out.y             == msg.y);
    REQUIRE(out.z             == msg.z);
    REQUIRE(out.yaw           == msg.yaw);
}

TEST_CASE("MsgHeartbeat size", "[protocol]")
{
    REQUIRE(sizeof(MsgHeartbeat) == 1);
}
