#include "PacketReader.hpp"

#include <cstring>
#include <stdexcept>

PacketReader::PacketReader(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_offset(0) { }

bool PacketReader::canRead(size_t count) const { return count <= m_size - m_offset; }

size_t PacketReader::remaining() const { return m_size - m_offset; }

size_t PacketReader::position() const { return m_offset; }

void PacketReader::skip(size_t count) {
    require(count);
    m_offset += count;
}

uint8_t PacketReader::readByte() {
    require(1);
    return m_data[m_offset++];
}

int8_t PacketReader::readInt8() { return static_cast<int8_t>(readByte()); }

int16_t PacketReader::readInt16() {
    return static_cast<int16_t>(
        (static_cast<uint16_t>(readByte()) << 8) |
        static_cast<uint16_t>(readByte())
        );
}

bool PacketReader::readBool() { return readByte() != 0; }

int16_t PacketReader::readShort() {
    require(2);

    const uint16_t value = (static_cast<uint16_t>(m_data[m_offset] ) << 8) | static_cast<uint16_t>(m_data[m_offset + 1]);
    m_offset += 2;

    return static_cast<int16_t>(value);
}

uint16_t PacketReader::readUnsignedShort() {
    require(2);

    const uint16_t value = (static_cast<uint16_t>(m_data[m_offset]) << 8) | static_cast<uint16_t>(m_data[m_offset + 1]);
    m_offset += 2;

    return value;
}

int32_t PacketReader::readInt() {
    require(4);

    const uint32_t value =
        (static_cast<uint32_t>(m_data[m_offset]) << 24) |
        (static_cast<uint32_t>(m_data[m_offset + 1]) << 16) |
        (static_cast<uint32_t>(m_data[m_offset + 2]) << 8) |
        static_cast<uint32_t>(m_data[m_offset + 3]);

    m_offset += 4;
    return static_cast<int32_t>(value);
}

int64_t PacketReader::readLong() {
    require(8);

    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value <<= 8;
        value |= m_data[m_offset + i];
    }
    m_offset += 8;

    return static_cast<int64_t>(value);
}

float PacketReader::readFloat() {
    const uint32_t raw = static_cast<uint32_t>(readInt());
    float value{};
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

double PacketReader::readDouble() {
    const uint64_t raw = static_cast<uint64_t>(readLong());
    double value{};
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

int32_t PacketReader::readVarInt() {
    int32_t result = 0;
    int shift = 0;

    while (true) {
        const uint8_t byte = readByte();
        result |= static_cast<int32_t>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) break;
        shift += 7;
        if (shift >= 35) throw std::runtime_error("VarInt too large");
    }

    return result;
}

int64_t PacketReader::readVarLong() {
    int64_t result = 0;
    int shift = 0;

    while (true) {
        const uint8_t byte = readByte();
        result |= static_cast<int64_t>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) break;
        shift += 7;
        if (shift >= 70) throw std::runtime_error("VarLong too large");
    }

    return result;
}

std::string PacketReader::readString(size_t maxLength) {
    const int32_t length = readVarInt();

    if (length < 0) throw std::runtime_error("Negative string length");
    if (static_cast<size_t>(length) > maxLength) throw std::runtime_error("String too long");

    require(static_cast<size_t>(length));
    std::string result(reinterpret_cast<const char*>(m_data + m_offset ), static_cast<size_t>(length));
    m_offset += static_cast<size_t>(length);

    return result;
}

std::vector<uint8_t> PacketReader::readBytes(size_t count) {
    require(count);
    std::vector<uint8_t> result( m_data + m_offset, m_data + m_offset + count);
    m_offset += count;
    
    return result;
}

void PacketReader::require(size_t count) const {
    if (!canRead(count)) throw std::runtime_error("Packet buffer underflow");
}