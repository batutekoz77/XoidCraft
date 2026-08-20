#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <winsock2.h>

#include "PacketWriter.hpp"
#include "PacketBuilder.hpp"
#include "PacketFramer.hpp"

#include "../Protocol/PacketHandler.hpp"
#include "../World/World.hpp"


struct PlayerInfo {
    int32_t entityId;
    std::string uuid;
    std::string username;

    double x;
    double y;
    double z;

    float yaw;
    float pitch;

    bool onGround;
};

struct DroppedItem {
    double x;
    double y;
    double z;

    int16_t itemId;
};

class TcpServer {
public:
    TcpServer();
    ~TcpServer();

    bool start(const char* address, unsigned short port);
    void run();

    void sendPacket(SOCKET clientSocket, int32_t packetId, const PacketWriter& writer);
    void sendJoinGame(SOCKET clientSocket, int32_t entityId);

    void sendSpawnPosition(SOCKET clientSocket, int32_t x, int32_t y, int32_t z);
    void sendChunkColumn(SOCKET clientSocket, int32_t chunkX, int32_t chunkZ);
    void sendPlayerPositionAndLook(SOCKET clientSocket, double x, double y, double z, float yaw, float pitch, uint8_t flags);

private:
    bool initializeWinsock();
    bool createSocket();
    bool bindSocket(const char* address, unsigned short port);
    bool listenSocket();

    void shutdown();

    void handleClient(SOCKET clientSocket);
    void handlePlayPacket(SOCKET clientSocket, const RawPacket& packet);

    void sendLoginSuccess(SOCKET clientSocket, PacketHandler& packetHandler, const std::string& username);

    void keepAliveLoop(SOCKET clientSocket, std::atomic<bool>& running);

    void broadcastBlockChange(int32_t x, int32_t y, int32_t z, uint8_t type, uint8_t meta);

    // --- Multiplayer movement senkronizasyonu ---
    void sendSpawnPlayer(SOCKET target, const PlayerInfo& player);
    void broadcastSpawnPlayer(const PlayerInfo& player, SOCKET exclude);
    void broadcastDestroyEntity(int32_t entityId, SOCKET exclude);
    void broadcastEntityTeleport(const PlayerInfo& player, SOCKET exclude);

    void handlePlayerPosition(SOCKET clientSocket, double x, double y, double z, bool onGround);
    void handlePlayerLook(SOCKET clientSocket, float yaw, float pitch, bool onGround);
    void handlePlayerPositionAndLook(SOCKET clientSocket, double x, double y, double z, float yaw, float pitch, bool onGround);

    static uint8_t angleToByte(float degrees);

    // --- Blok kirma sonrasi item dusurme ---
    uint8_t getBlockType(int32_t x, int32_t y, int32_t z) const;
    void spawnDroppedItem(int32_t x, int32_t y, int32_t z, int16_t itemId);
    void checkItemPickups(const PlayerInfo& player);
    void broadcastCollectItem(int32_t itemEntityId, int32_t collectorEntityId);

    // --- Blok yerlestirme ---
    void handleBlockPlacement(SOCKET clientSocket, int32_t x, int32_t y, int32_t z, int8_t face, int16_t itemId);

    void sendStatusResponse(SOCKET clientSocket);
    void sendPong(SOCKET clientSocket, int64_t payload);

private:
    SOCKET m_serverSocket;
    bool m_running;

    World m_world;

    std::atomic<int32_t> m_nextEntityId{ 1 };

    std::vector<SOCKET> m_clients;
    std::unordered_map<SOCKET, PlayerInfo> m_players;
    std::mutex m_clientsMutex;

    std::unordered_map<int32_t, DroppedItem> m_droppedItems;
    std::mutex m_itemsMutex;

    std::mutex m_sendMutex;
};