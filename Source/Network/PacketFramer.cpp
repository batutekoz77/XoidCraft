#include "PacketFramer.hpp"
#include "PacketReader.hpp"

#include <stdexcept>


void PacketFramer::append(const uint8_t* data, size_t size) {
    m_buffer.insert(m_buffer.end(), data, data + size);
}

bool PacketFramer::nextPacket(RawPacket& packet) {
    int32_t packetLength = 0;
    size_t lengthBytes = 0;

    if (!tryReadVarInt(0, packetLength, lengthBytes)) return false;
    if (packetLength < 0) throw std::runtime_error("Negative packet length");

    const size_t totalSize = lengthBytes + static_cast<size_t>(packetLength);

    if (m_buffer.size() < totalSize) return false;

    const uint8_t* packetData = m_buffer.data() + lengthBytes;

    PacketReader reader(packetData, static_cast<size_t>(packetLength));
    packet.id = reader.readVarInt();
    packet.payload = reader.readBytes(reader.remaining());
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + totalSize);

    return true;
}

void PacketFramer::clear() { m_buffer.clear(); }

size_t PacketFramer::bufferedBytes() const { return m_buffer.size(); }

bool PacketFramer::tryReadVarInt(size_t start, int32_t& value, size_t& bytes) const {
    value = 0;
    bytes = 0;

    int shift = 0;

    while (true) {
        if (start + bytes >= m_buffer.size()) return false;

        const uint8_t byte = m_buffer[start + bytes];
        value |= static_cast<int32_t>(byte & 0x7F) << shift;
        ++bytes;

        if ((byte & 0x80) == 0) return true;
        shift += 7;
        if (shift >= 35) throw std::runtime_error("Packet length VarInt too large");
    }
}