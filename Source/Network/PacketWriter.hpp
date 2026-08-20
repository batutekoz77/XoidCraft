#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class PacketWriter {
public:
    void writeByte(uint8_t value);
    void writeBool(bool value);

    void writeShort(int16_t value);
    void writeUnsignedShort(uint16_t value);

    void writeInt(int32_t value);
    void writeLong(int64_t value);

    void writeFloat(float value);
    void writeDouble(double value);

    void writeVarInt(int32_t value);
    void writeVarLong(int64_t value);

    void writeString(const std::string& value);
    void writeBytes(const uint8_t* data, size_t size);
    void writeBytes(const std::vector<uint8_t>& data);

    const std::vector<uint8_t>& data() const;
    size_t size() const;

    void clear();

private:
    std::vector<uint8_t> m_data;
};