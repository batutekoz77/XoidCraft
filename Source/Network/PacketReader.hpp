#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class PacketReader {
    public:
        PacketReader(const uint8_t* data, size_t size);

        bool canRead(size_t count) const;

        size_t remaining() const;
        size_t position() const;

        void skip(size_t count);

        uint8_t readByte();
        int8_t readInt8();
        int16_t readInt16();

        bool readBool();

        int16_t readShort();
        uint16_t readUnsignedShort();

        int32_t readInt();
        int64_t readLong();

        float readFloat();
        double readDouble();

        int32_t readVarInt();
        int64_t readVarLong();

        std::string readString(size_t maxLength = 32767);
        std::vector<uint8_t> readBytes(size_t count);

    private:
        void require(size_t count) const;

    private:
        const uint8_t* m_data;
        size_t m_size;
        size_t m_offset;
};