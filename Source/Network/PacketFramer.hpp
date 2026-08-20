#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct RawPacket {
    int32_t id;
    std::vector<uint8_t> payload;
};

class PacketFramer {
    public:
        void append(const uint8_t* data, size_t size);
        bool nextPacket(RawPacket& packet);
        void clear();
        size_t bufferedBytes() const;

    private:
        bool tryReadVarInt(size_t start, int32_t& value, size_t& bytes) const;

    private:
        std::vector<uint8_t> m_buffer;
};