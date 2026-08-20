#include "PacketHandler.hpp"

#include <cstdio>

#include "../Protocol/Position.hpp"
#include "../Logger/Logger.hpp"


PacketHandler::PacketHandler() : m_state(ConnectionState::Handshaking) {}

void PacketHandler::reset() {
    m_state = ConnectionState::Handshaking;
    m_username.clear();
}

void PacketHandler::handlePacket(const RawPacket& packet) {
    Logger::Debug("[Packet] ID: {} | Payload: {} bytes", packet.id, packet.payload.size());

    std::string hexData;

    for (uint8_t byte : packet.payload) {
        char buffer[4]{};
        std::snprintf(buffer, sizeof(buffer), "%02X ", byte);
        hexData += buffer;
    }

    if (!hexData.empty()) hexData.pop_back();

    Logger::Debug("[Packet] Data: {}", hexData);

    switch (m_state) {
    case ConnectionState::Handshaking:
        handleHandshaking(packet);
        break;

    case ConnectionState::Status:
        handleStatus(packet);
        break;

    case ConnectionState::Login:
        handleLogin(packet);
        break;

    case ConnectionState::Play:
        switch (packet.id) {
        case 0x00: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const int32_t keepAliveId = reader.readVarInt();
                Logger::Debug("[Play] Keep Alive: ID={}", keepAliveId);
            }
            catch (const std::exception& e) { Logger::Error("[Play] Keep Alive parse error: {}", e.what()); }

            break;
        }

        case 0x01: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const std::string message = reader.readString();
                Logger::Info("[Play] Chat Message: {}", message);
            }
            catch (const std::exception& e) { Logger::Error("[Play] Chat Message parse error: {}", e.what()); }

            break;
        }

        case 0x02: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const int32_t entityId = reader.readVarInt();
                const int32_t action = reader.readVarInt();

                switch (action) {
                case 0:
                    Logger::Info("[Play] Use Entity: Interact Entity={}", entityId);
                    break;

                case 1:
                    Logger::Info("[Play] Use Entity: Attack Entity={}", entityId);
                    break;

                case 2: {
                    const float targetX = reader.readFloat();
                    const float targetY = reader.readFloat();
                    const float targetZ = reader.readFloat();

                    Logger::Info(
                        "[Play] Use Entity: Interact At Entity={} X={} Y={} Z={}",
                        entityId,
                        targetX,
                        targetY,
                        targetZ
                    );

                    break;
                }

                default:
                    Logger::Warn("[Play] Use Entity: Unknown Action={} Entity={}", action, entityId);
                    break;
                }
            }
            catch (const std::exception& e) { Logger::Error("[Play] Use Entity parse error: {}", e.what()); }

            break;
        }

        case 0x03: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const bool onGround = reader.readBool();
                Logger::Debug("[Play] Player: OnGround={}", onGround);
            }
            catch (const std::exception& e) { Logger::Error("[Play] Player parse error: {}", e.what()); }

            break;
        }

        case 0x04: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const double x = reader.readDouble();
                const double y = reader.readDouble();
                const double z = reader.readDouble();
                const bool onGround = reader.readBool();

                Logger::Debug(
                    "[Play] Player Position: X={} Y={} Z={} OnGround={}",
                    x,
                    y,
                    z,
                    onGround
                );
            }
            catch (const std::exception& e) { Logger::Error("[Play] Player Position parse error: {}", e.what()); }

            break;
        }

        case 0x05: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const float yaw = reader.readFloat();
                const float pitch = reader.readFloat();
                const bool onGround = reader.readBool();

                Logger::Debug(
                    "[Play] Player Look: Yaw={} Pitch={} OnGround={}",
                    yaw,
                    pitch,
                    onGround
                );
            }
            catch (const std::exception& e) { Logger::Error("[Play] Player Look parse error: {}", e.what()); }

            break;
        }

        case 0x06: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const double x = reader.readDouble();
                const double y = reader.readDouble();
                const double z = reader.readDouble();
                const float yaw = reader.readFloat();
                const float pitch = reader.readFloat();
                const bool onGround = reader.readBool();

                Logger::Debug(
                    "[Play] Player Position And Look: X={} Y={} Z={} Yaw={} Pitch={} OnGround={}",
                    x,
                    y,
                    z,
                    yaw,
                    pitch,
                    onGround
                );
            }
            catch (const std::exception& e) { Logger::Error("[Play] Player Position And Look parse error: {}", e.what()); }

            break;
        }

        case 0x07: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const int8_t status = reader.readInt8();
                const int64_t positionRaw = reader.readLong();
                const int8_t face = reader.readInt8();

                int32_t x, y, z;
                Position::decode(positionRaw, x, y, z);

                switch (status) {
                case 0:
                    Logger::Info("[Play] Digging: Start ({}, {}, {}) Face={}", x, y, z, face);
                    break;

                case 1:
                    Logger::Info("[Play] Digging: Cancel ({}, {}, {}) Face={}", x, y, z, face);
                    break;

                case 2:
                    Logger::Info("[Play] Digging: Finish ({}, {}, {}) Face={}", x, y, z, face);
                    break;

                case 3:
                    Logger::Info("[Play] Digging: Drop Item Stack");
                    break;

                case 4:
                    Logger::Info("[Play] Digging: Drop Item");
                    break;

                case 5:
                    Logger::Info("[Play] Digging: Shoot Arrow / Finish Eating");
                    break;

                case 6:
                    Logger::Info("[Play] Digging: Swap Item In Hand");
                    break;

                default:
                    Logger::Warn("[Play] Digging: Unknown Status={}", status);
                    break;
                }
            }
            catch (const std::exception& e) { Logger::Error("[Play] Digging parse error: {}", e.what()); }

            break;
        }

        case 0x08:
            Logger::Info("[Play] Player Block Placement");
            break;

        case 0x09: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const int16_t slot = reader.readInt16();

                Logger::Info("[Play] Held Item Change: Slot={}", slot);
            }
            catch (const std::exception& e) { Logger::Error("[Play] Held Item Change parse error: {}", e.what()); }

            break;
        }

        case 0x0A:
            Logger::Debug("[Play] Animation");
            break;

        case 0x0B: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const int32_t entityId = reader.readVarInt();
                const int32_t action = reader.readVarInt();
                const int32_t actionParameter = reader.readVarInt();

                switch (action) {
                case 0:
                    Logger::Info("[Play] Entity Action: Start Sneaking");
                    break;

                case 1:
                    Logger::Info("[Play] Entity Action: Stop Sneaking");
                    break;

                case 2:
                    Logger::Info("[Play] Entity Action: Leave Bed");
                    break;

                case 3:
                    Logger::Info("[Play] Entity Action: Start Sprinting");
                    break;

                case 4:
                    Logger::Info("[Play] Entity Action: Stop Sprinting");
                    break;

                case 5:
                    Logger::Info("[Play] Entity Action: Start Jump With Horse");
                    break;

                case 6:
                    Logger::Info("[Play] Entity Action: Open Inventory");
                    break;

                default:
                    Logger::Warn(
                        "[Play] Entity Action: Unknown Action={} Parameter={} Entity={}",
                        action,
                        actionParameter,
                        entityId
                    );
                    break;
                }
            }
            catch (const std::exception& e) {
                Logger::Error("[Play] Entity Action parse error: {}", e.what());
            }

            break;
        }

        case 0x0C: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const float sideways = reader.readFloat();
                const float forward = reader.readFloat();
                const int8_t flags = reader.readInt8();

                Logger::Info(
                    "[Play] Steer Vehicle: Sideways={} Forward={} Flags={}",
                    sideways,
                    forward,
                    flags
                );
            }
            catch (const std::exception& e) {
                Logger::Error("[Play] Steer Vehicle parse error: {}", e.what());
            }

            break;
        }

        case 0x0D: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const uint8_t windowId = reader.readInt8();

                Logger::Debug("[Play] Close Window: Window={}", windowId);
            }
            catch (const std::exception& e) {
                Logger::Error("[Play] Close Window parse error: {}", e.what());
            }

            break;
        }

        case 0x0E:
            Logger::Info("[Play] Click Window");
            break;

        case 0x0F:
            break;

        case 0x10:
            Logger::Info("[Play] Creative Inventory Action");
            break;

        case 0x11: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const uint8_t windowId = reader.readInt8();
                const uint8_t enchantment = reader.readInt8();

                Logger::Info(
                    "[Play] Enchant Item: Window={} Enchantment={}",
                    windowId,
                    enchantment
                );
            }
            catch (const std::exception& e) {
                Logger::Error("[Play] Enchant Item parse error: {}", e.what());
            }

            break;
        }

        case 0x12:
            Logger::Info("[Play] Update Sign");
            break;

        case 0x13: {
            PacketReader reader(packet.payload.data(), packet.payload.size());

            try {
                const uint8_t flags = reader.readInt8();
                const float flyingSpeed = reader.readFloat();
                const float walkingSpeed = reader.readFloat();

                Logger::Info(
                    "[Play] Player Abilities: Flags={} FlyingSpeed={} WalkingSpeed={}",
                    flags,
                    flyingSpeed,
                    walkingSpeed
                );
            }
            catch (const std::exception& e) { Logger::Error("[Play] Player Abilities parse error: {}", e.what()); }

            break;
        }

        case 0x14:
            Logger::Info("[Play] Tab Complete");
            break;

        case 0x15:
            Logger::Info("[Play] Client Settings");
            break;

        case 0x16:
            break;

        case 0x17:
            Logger::Info("[Play] Plugin Message");
            break;

        case 0x18:
            Logger::Info("[Play] Update Sign");
            break;

        case 0x19:
            Logger::Info("[Play] Resource Pack Status");
            break;

        case 0x1A:
            Logger::Info("[Play] Spectate");
            break;

        case 0x1B:
            Logger::Info("[Play] Player Block Placement");
            break;

        case 0x1C:
            Logger::Info("[Play] Use Item");
            break;

        default:
            Logger::Debug("[Packet] Unknown Play packet: ID {} | Payload: {} bytes", packet.id, packet.payload.size());
            break;
        }

        break;
    }
}

ConnectionState PacketHandler::getState() const { return m_state; }
void PacketHandler::setState(ConnectionState state) { m_state = state; }

const std::string& PacketHandler::getUsername() const { return m_username; }

void PacketHandler::handleHandshaking(const RawPacket& packet) {
    if (packet.id != 0x00) {
        Logger::Warn("[Protocol] Unexpected handshake packet: {}", packet.id);
        return;
    }

    PacketReader reader(packet.payload.data(), packet.payload.size());

    try {
        const int32_t protocolVersion = reader.readVarInt();
        const std::string serverAddress = reader.readString();
        const uint16_t serverPort = reader.readUnsignedShort();
        const int32_t nextState = reader.readVarInt();

        Logger::Debug("[Handshake]");
        Logger::Debug("  Protocol Version: {}", protocolVersion);
        Logger::Debug("  Server Address: {}", serverAddress);
        Logger::Debug("  Server Port: {}", serverPort);
        Logger::Debug("  Next State: {}", nextState);

        switch (nextState) {
        case 1:
            m_state = ConnectionState::Status;
            Logger::Debug("[Protocol] State -> STATUS");
            break;

        case 2:
            m_state = ConnectionState::Login;
            Logger::Info("[Protocol] State -> LOGIN");
            break;

        default:
            Logger::Warn("[Protocol] Invalid next state: {}", nextState);
            break;
        }
    }
    catch (const std::exception& e) { Logger::Error("[Protocol] Handshake parse error: {}", e.what()); }
}

void PacketHandler::handleStatus(const RawPacket& packet) {
    Logger::Debug("[Status] Packet ID: {}", packet.id);
}

void PacketHandler::handleLogin(const RawPacket& packet) {
    if (packet.id != 0x00) {
        Logger::Warn("[Login] Unexpected packet ID: {}", packet.id);
        return;
    }

    PacketReader reader(packet.payload.data(), packet.payload.size());

    try {
        m_username = reader.readString();
        Logger::Info("[Login] Username: {}", m_username);
    }
    catch (const std::exception& e) { Logger::Error("[Login] Parse error: {}", e.what()); }
}
