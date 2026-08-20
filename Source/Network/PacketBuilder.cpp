#include "PacketBuilder.hpp"

#include <cstddef>

std::vector<uint8_t> PacketBuilder::build(int32_t packetId, const PacketWriter& writer) {
    PacketWriter packet;
    packet.writeVarInt(packetId);
    packet.writeBytes(writer.data());

    PacketWriter result;
    result.writeVarInt(static_cast<int32_t>(packet.size()));
    result.writeBytes(packet.data());

    return result.data();
}