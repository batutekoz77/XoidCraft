#pragma once

#include <string>

#include "../Network/PacketFramer.hpp"
#include "../Network/PacketReader.hpp"

#include "ConnectionState.hpp"

class PacketHandler {
    public:
        PacketHandler();
        void reset();
        void handlePacket(const RawPacket& packet);

        ConnectionState getState() const;
        void setState(ConnectionState state);

        const std::string& getUsername() const;

    private:
        void handleHandshaking(const RawPacket& packet);
        void handleStatus(const RawPacket& packet);
        void handleLogin(const RawPacket& packet);

    private:
        ConnectionState m_state;
        std::string m_username;
};