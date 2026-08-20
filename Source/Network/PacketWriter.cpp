#include "PacketWriter.hpp"

#include <cstring>

void PacketWriter::writeByte(uint8_t value) { m_data.push_back(value); }

void PacketWriter::writeBool(bool value) { writeByte(value ? 1 : 0); }

void PacketWriter::writeShort(int16_t value) {
    const uint16_t raw = static_cast<uint16_t>(value);

    m_data.push_back(static_cast<uint8_t>((raw >> 8) & 0xFF));
    m_data.push_back(static_cast<uint8_t>(raw & 0xFF));
}

void PacketWriter::writeUnsignedShort(uint16_t value) {
    m_data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    m_data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PacketWriter::writeInt(int32_t value) {
    const uint32_t raw = static_cast<uint32_t>(value);

    m_data.push_back(static_cast<uint8_t>((raw >> 24) & 0xFF));
    m_data.push_back(static_cast<uint8_t>((raw >> 16) & 0xFF));
    m_data.push_back(static_cast<uint8_t>((raw >> 8) & 0xFF));
    m_data.push_back(static_cast<uint8_t>(raw & 0xFF));
}

void PacketWriter::writeLong(int64_t value) {
    const uint64_t raw = static_cast<uint64_t>(value);

    for (int i = 7; i >= 0; --i) {
        m_data.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xFF));
    }
}

void PacketWriter::writeFloat(float value) {
    uint32_t raw{};

    std::memcpy(&raw, &value, sizeof(value));

    writeInt(static_cast<int32_t>(raw));
}

void PacketWriter::writeDouble(double value) {
    uint64_t raw{};

    std::memcpy(&raw, &value, sizeof(value));

    writeLong(static_cast<int64_t>(raw));
}

void PacketWriter::writeVarInt(int32_t value) {
    uint32_t raw = static_cast<uint32_t>(value);

    while (true) {
        if ((raw & ~0x7FU) == 0) {
            m_data.push_back(static_cast<uint8_t>(raw));
            return;
        }

        m_data.push_back(static_cast<uint8_t>((raw & 0x7F) | 0x80));
        raw >>= 7;
    }
}

void PacketWriter::writeVarLong(int64_t value) {
    uint64_t raw = static_cast<uint64_t>(value);

    while (true) {
        if ((raw & ~0x7FULL) == 0) {
            m_data.push_back(static_cast<uint8_t>(raw));
            return;
        }

        m_data.push_back(static_cast<uint8_t>((raw & 0x7F) | 0x80));
        raw >>= 7;
    }
}

void PacketWriter::writeString(const std::string& value) {
    writeVarInt(static_cast<int32_t>(value.size()));
    writeBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

void PacketWriter::writeBytes(const uint8_t* data, size_t size) { m_data.insert(m_data.end(), data, data + size); }

void PacketWriter::writeBytes(const std::vector<uint8_t>& data) { m_data.insert(m_data.end(), data.begin(), data.end()); }

const std::vector<uint8_t>& PacketWriter::data() const { return m_data; }

size_t PacketWriter::size() const { return m_data.size(); }

void PacketWriter::clear() { m_data.clear(); }