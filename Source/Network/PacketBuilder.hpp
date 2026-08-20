#pragma once

#include <cstdint>
#include <vector>

#include "PacketWriter.hpp"

class PacketBuilder {
public:
    static std::vector<uint8_t> build(
        int32_t packetId,
        const PacketWriter& writer
    );
};