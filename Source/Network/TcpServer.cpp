#include <string>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <ws2tcpip.h>
#include <objbase.h>

#include "TcpServer.hpp"
#include "PacketBuilder.hpp"

#include "../Main/Config.hpp"
#include "../Protocol/Position.hpp"
#include "../Logger/Logger.hpp"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")

namespace {
    // Kirilan blok tipine gore dusecek esya ID'sini dondurur. -1 => hicbir sey dusmez.
    int16_t droppedItemFor(uint8_t blockType) {
        switch (blockType) {
        case 1: return 4;  // Tas -> Cakil Tasi (Cobblestone)
        case 2: return 3;  // Cimen -> Toprak (Dirt)
        case 3: return 3;  // Toprak -> Toprak (Dirt)
        case 7: return -1; // Bedrock kirilamaz
        default: return static_cast<int16_t>(blockType);
        }
    }

    struct SlotData {
        int16_t itemId = -1;
        uint8_t count = 0;
        int16_t damage = 0;
    };

    void skipNbtValue(PacketReader& reader, uint8_t tagType);

    // Bir NBT compound govdesini (isim+deger ciftleri, TAG_End ile biter) atlar.
    void skipNbtCompound(PacketReader& reader) {
        while (true) {
            const uint8_t type = reader.readByte();
            if (type == 0) break; // TAG_End

            const uint16_t nameLength = reader.readUnsignedShort();
            reader.skip(nameLength);

            skipNbtValue(reader, type);
        }
    }

    // Tek bir NBT etiket degerini (adi zaten okunmus varsayilir) atlar.
    void skipNbtValue(PacketReader& reader, uint8_t tagType) {
        switch (tagType) {
        case 1: reader.skip(1); break;  // Byte
        case 2: reader.skip(2); break;  // Short
        case 3: reader.skip(4); break;  // Int
        case 4: reader.skip(8); break;  // Long
        case 5: reader.skip(4); break;  // Float
        case 6: reader.skip(8); break;  // Double
        case 7: { const int32_t length = reader.readInt(); reader.skip(static_cast<size_t>(length)); break; } // Byte Array
        case 8: { const uint16_t length = reader.readUnsignedShort(); reader.skip(length); break; } // String
        case 9: { // List
            const uint8_t elementType = reader.readByte();
            const int32_t count = reader.readInt();
            for (int32_t i = 0; i < count; ++i) skipNbtValue(reader, elementType);
            break;
        }
        case 10: skipNbtCompound(reader); break; // Compound
        case 11: { const int32_t length = reader.readInt(); reader.skip(static_cast<size_t>(length) * 4); break; } // Int Array
        default: break;
        }
    }

    // Player Block Placement paketindeki Slot alanini okur; NBT varsa dogru sekilde atlar
    // (aksi halde sonraki Cursor X/Y/Z alanlari yanlis yerden okunur).
    SlotData readSlot(PacketReader& reader) {
        SlotData slot{};
        slot.itemId = reader.readInt16();

        if (slot.itemId != -1) {
            slot.count = reader.readByte();
            slot.damage = reader.readInt16();

            const uint8_t nbtType = reader.readByte();
            if (nbtType != 0) skipNbtValue(reader, nbtType);
        }

        return slot;
    }
}


TcpServer::TcpServer() : m_serverSocket(INVALID_SOCKET), m_running(false), m_world(OUTPUT_DIRECTORY() + "\\World\\world.dat") {
    m_world.load();
    m_world.startAutoSave();
}
TcpServer::~TcpServer() { shutdown(); }

bool TcpServer::start(const char* address, unsigned short port) {
    Logger::Info("[Server] Starting...");
    if (!initializeWinsock()) return false;
    if (!createSocket()) return false;
    if (!bindSocket(address, port)) return false;
    if (!listenSocket()) return false;

    m_running = true;
    Logger::Info("[Server] Listening on {}:{}", address, port);
    return true;
}

bool TcpServer::initializeWinsock() {
    WSADATA wsaData{};
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0) {
        Logger::Error("[Network] WSAStartup failed: {}", result);
        return false;
    }

    Logger::Info("[Network] Winsock initialized.");
    return true;
}

bool TcpServer::createSocket() {
    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (m_serverSocket == INVALID_SOCKET) {
        Logger::Error("[Network] Socket creation failed: {}", WSAGetLastError());
        return false;
    }

    Logger::Info("[Network] TCP socket created.");
    return true;
}

bool TcpServer::bindSocket(const char* address, unsigned short port) {
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    const int result = inet_pton(AF_INET, address, &serverAddress.sin_addr);

    if (result != 1) {
        Logger::Error("[Network] Invalid address.");
        return false;
    }

    if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        Logger::Error("[Network] Bind failed: {}", WSAGetLastError());
        return false;
    }

    Logger::Info("[Network] Bound to {}:{}", address, port);
    return true;
}

bool TcpServer::listenSocket() {
    if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::Error("[Network] Listen failed: {}", WSAGetLastError());
        return false;
    }
    return true;
}

void TcpServer::handleClient(SOCKET clientSocket) {
    /* {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.push_back(clientSocket);
    }*/

    PacketFramer packetFramer;
    PacketHandler packetHandler;

    std::atomic<bool> keepAliveRunning{ false };
    std::thread keepAliveThread;

    uint8_t buffer[8192];

    while (true) {
        const int received = recv(clientSocket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);

        if (received == 0) break;
        if (received == SOCKET_ERROR) {
            Logger::Error("[Network] recv failed: {}", WSAGetLastError());
            break;
        }

        packetFramer.append(buffer, static_cast<size_t>(received));

        RawPacket packet;
        while (packetFramer.nextPacket(packet)) {
            const ConnectionState previousState = packetHandler.getState();

            packetHandler.handlePacket(packet);

            if (previousState == ConnectionState::Status) {
                if (packet.id == 0x00) sendStatusResponse(clientSocket);
                else if (packet.id == 0x01) {
                    PacketReader reader(packet.payload.data(), packet.payload.size());

                    try {
                        const int64_t payload = reader.readLong();
                        sendPong(clientSocket, payload);
                    }
                    catch (const std::exception& e) { Logger::Error("[Status] Ping parse error: {}", e.what()); }
                }
            }

            if (previousState == ConnectionState::Login &&
                packet.id == 0x00 &&
                packetHandler.getState() == ConnectionState::Login &&
                !packetHandler.getUsername().empty()) {

                sendLoginSuccess(clientSocket, packetHandler, packetHandler.getUsername());

                keepAliveRunning = true;

                keepAliveThread = std::thread(
                    &TcpServer::keepAliveLoop,
                    this,
                    clientSocket,
                    std::ref(keepAliveRunning)
                );
            }

            if (packetHandler.getState() == ConnectionState::Play) {
                handlePlayPacket(clientSocket, packet);
            }
        }
    }

    keepAliveRunning = false;
    if (keepAliveThread.joinable()) {
        keepAliveThread.join();
    }

    int32_t disconnectedEntityId = -1;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), clientSocket), m_clients.end());

        const auto it = m_players.find(clientSocket);
        if (it != m_players.end()) {
            disconnectedEntityId = it->second.entityId;
            m_players.erase(it);
        }
    }

    if (disconnectedEntityId != -1) broadcastDestroyEntity(disconnectedEntityId, clientSocket);

    closesocket(clientSocket);
    Logger::Info("[Network] Client disconnected.");
}

void TcpServer::run() {
    Logger::Info("[Server] Waiting for connections...");

    while (m_running) {
        SOCKET clientSocket = accept(m_serverSocket, nullptr, nullptr);

        if (clientSocket == INVALID_SOCKET) {
            if (!m_running) break;
            Logger::Error("[Network] Accept failed: {}", WSAGetLastError());
            continue;
        }

        Logger::Info("[Network] Client connected.");
        std::thread(&TcpServer::handleClient, this, clientSocket).detach();
    }
}

void TcpServer::sendPacket(SOCKET clientSocket, int32_t packetId, const PacketWriter& writer) {
    std::lock_guard<std::mutex> lock(m_sendMutex);

    const std::vector<uint8_t> packet = PacketBuilder::build(packetId, writer);
    size_t totalSent = 0;

    while (totalSent < packet.size()) {
        const int sent = send(clientSocket, reinterpret_cast<const char*>(packet.data() + totalSent), static_cast<int>(packet.size() - totalSent), 0);

        if (sent == SOCKET_ERROR) {
            Logger::Error("[Network] Send failed: {}", WSAGetLastError());
            return;
        }

        totalSent += static_cast<size_t>(sent);
    }
}

void TcpServer::keepAliveLoop(SOCKET clientSocket, std::atomic<bool>& running) {
    using namespace std::chrono_literals;

    while (running) {
        for (int i = 0; i < 150 && running; ++i) {
            std::this_thread::sleep_for(100ms);
        }
        if (!running) break;

        const int32_t keepAliveId = static_cast<int32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
            );

        PacketWriter writer;
        writer.writeVarInt(keepAliveId);
        sendPacket(clientSocket, 0x00, writer);
    }
}

void TcpServer::sendJoinGame(SOCKET clientSocket, int32_t entityId) {
    PacketWriter writer;
    writer.writeInt(entityId);
    writer.writeByte(0);
    writer.writeByte(0);
    writer.writeByte(1);
    writer.writeByte(20);
    writer.writeString("default");
    writer.writeBool(false);

    sendPacket(clientSocket, 0x01, writer);
}

void TcpServer::sendChunkColumn(SOCKET clientSocket, int32_t chunkX, int32_t chunkZ) {
    std::vector<uint8_t> blocks(8192, 0);

    auto setBlock = [&](int x, int y, int z, uint8_t type) {
        const int index = (y << 8) | (z << 4) | x;
        blocks[index * 2] = static_cast<uint8_t>((type << 4) & 0xFF);
        blocks[index * 2 + 1] = static_cast<uint8_t>(type >> 4);
        };

    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            setBlock(x, 0, z, 7);
            setBlock(x, 1, z, 1);
            setBlock(x, 2, z, 1);
            setBlock(x, 3, z, 1);
            setBlock(x, 4, z, 3);
            setBlock(x, 5, z, 3);
            setBlock(x, 6, z, 3);
            setBlock(x, 7, z, 2);

            // YENİ: kayıtlı dünya değişikliklerini üstüne uygula
            for (int y = 0; y < 16; ++y) {
                BlockData override{};
                const int32_t worldX = chunkX * 16 + x;
                const int32_t worldZ = chunkZ * 16 + z;

                if (m_world.getOverride(worldX, y, worldZ, override)) {
                    setBlock(x, y, z, override.type); // meta'yı basitlik için 0 varsayıyoruz
                }
            }
        }
    }

    const std::vector<uint8_t> blockLight(2048, 0x00);
    const std::vector<uint8_t> skyLight(2048, 0xFF);
    const std::vector<uint8_t> biomes(256, 1);

    std::vector<uint8_t> data;
    data.reserve(blocks.size() + blockLight.size() + skyLight.size() + biomes.size());
    data.insert(data.end(), blocks.begin(), blocks.end());
    data.insert(data.end(), blockLight.begin(), blockLight.end());
    data.insert(data.end(), skyLight.begin(), skyLight.end());
    data.insert(data.end(), biomes.begin(), biomes.end());

    PacketWriter writer;
    writer.writeInt(chunkX);
    writer.writeInt(chunkZ);
    writer.writeBool(true);
    writer.writeUnsignedShort(0x0001);
    writer.writeVarInt(static_cast<int32_t>(data.size()));
    writer.writeBytes(data);

    sendPacket(clientSocket, 0x21, writer);
}

void TcpServer::sendSpawnPosition(SOCKET clientSocket, int32_t x, int32_t y, int32_t z) {
    PacketWriter writer;
    const int64_t encoded = ((static_cast<int64_t>(x) & 0x3FFFFFF) << 38)
        | ((static_cast<int64_t>(y) & 0xFFF) << 26)
        | (static_cast<int64_t>(z) & 0x3FFFFFF);
    writer.writeLong(encoded);
    sendPacket(clientSocket, 0x05, writer);
}

void TcpServer::sendPlayerPositionAndLook(SOCKET clientSocket, double x, double y, double z, float yaw, float pitch, uint8_t flags) {
    PacketWriter writer;
    writer.writeDouble(x);
    writer.writeDouble(y);
    writer.writeDouble(z);
    writer.writeFloat(yaw);
    writer.writeFloat(pitch);
    writer.writeByte(flags);
    sendPacket(clientSocket, 0x08, writer);
}

void TcpServer::sendLoginSuccess(SOCKET clientSocket, PacketHandler& packetHandler, const std::string& username) {
    GUID guid{};
    if (CoCreateGuid(&guid) != S_OK) {
        Logger::Error("[Login] Failed to generate UUID.");
        return;
    }

    wchar_t wideUuid[64]{};
    StringFromGUID2(guid, wideUuid, 64);
    std::wstring uuidWide(wideUuid);

    if (!uuidWide.empty() && uuidWide.front() == L'{') uuidWide.erase(0, 1);
    if (!uuidWide.empty() && uuidWide.back() == L'}') uuidWide.pop_back();

    std::string uuid(uuidWide.begin(), uuidWide.end());

    PacketWriter writer;
    writer.writeString(uuid);
    writer.writeString(username);

    sendPacket(clientSocket, 0x02, writer);

    const int32_t entityId = m_nextEntityId.fetch_add(1);
    sendJoinGame(clientSocket, entityId);

    packetHandler.setState(ConnectionState::Play);
    Logger::Info("[Protocol] State -> PLAY (EID {})", entityId);

    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            sendChunkColumn(clientSocket, cx, cz);
        }
    }

    sendSpawnPosition(clientSocket, 8, 8, 8);
    sendPlayerPositionAndLook(clientSocket, 8.5, 9.0, 8.5, 0.0f, 0.0f, 0);

    PlayerInfo newPlayer{};
    newPlayer.entityId = entityId;
    newPlayer.uuid = uuid;
    newPlayer.username = username;
    newPlayer.x = 8.5;
    newPlayer.y = 9.0;
    newPlayer.z = 8.5;
    newPlayer.yaw = 0.0f;
    newPlayer.pitch = 0.0f;
    newPlayer.onGround = false;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);

        // Yeni istemciye zaten bağlı olan diğer oyuncuları göster
        for (const auto& [socket, player] : m_players) sendSpawnPlayer(clientSocket, player);

        m_clients.push_back(clientSocket);
        m_players[clientSocket] = newPlayer;
    }

    // Yeni oyuncuyu diğer herkese göster
    broadcastSpawnPlayer(newPlayer, clientSocket);
}

void TcpServer::handlePlayPacket(SOCKET clientSocket, const RawPacket& packet) {
    switch (packet.id) {
    case 0x04: { // Player Position
        PacketReader reader(packet.payload.data(), packet.payload.size());

        try {
            const double x = reader.readDouble();
            const double y = reader.readDouble();
            const double z = reader.readDouble();
            const bool onGround = reader.readBool();

            handlePlayerPosition(clientSocket, x, y, z, onGround);
        }
        catch (const std::exception& e) {
            Logger::Error("[Play] Player Position parse error: {}", e.what());
        }

        break;
    }

    case 0x05: { // Player Look
        PacketReader reader(packet.payload.data(), packet.payload.size());

        try {
            const float yaw = reader.readFloat();
            const float pitch = reader.readFloat();
            const bool onGround = reader.readBool();

            handlePlayerLook(clientSocket, yaw, pitch, onGround);
        }
        catch (const std::exception& e) {
            Logger::Error("[Play] Player Look parse error: {}", e.what());
        }

        break;
    }

    case 0x06: { // Player Position And Look
        PacketReader reader(packet.payload.data(), packet.payload.size());

        try {
            const double x = reader.readDouble();
            const double y = reader.readDouble();
            const double z = reader.readDouble();
            const float yaw = reader.readFloat();
            const float pitch = reader.readFloat();
            const bool onGround = reader.readBool();

            handlePlayerPositionAndLook(clientSocket, x, y, z, yaw, pitch, onGround);
        }
        catch (const std::exception& e) {
            Logger::Error("[Play] Player Position And Look parse error: {}", e.what());
        }

        break;
    }

    case 0x08: { // Player Block Placement
        PacketReader reader(packet.payload.data(), packet.payload.size());

        try {
            const int64_t positionRaw = reader.readLong();
            const int8_t face = reader.readInt8();
            const SlotData heldItem = readSlot(reader);
            reader.readInt8(); // Cursor X, kullanmiyoruz
            reader.readInt8(); // Cursor Y, kullanmiyoruz
            reader.readInt8(); // Cursor Z, kullanmiyoruz

            int32_t x, y, z;
            Position::decode(positionRaw, x, y, z);

            handleBlockPlacement(clientSocket, x, y, z, face, heldItem.itemId);
        }
        catch (const std::exception& e) {
            Logger::Error("[Play] Block Placement parse error: {}", e.what());
        }

        break;
    }

    case 0x07: { // Player Digging
        PacketReader reader(packet.payload.data(), packet.payload.size());

        try {
            const int32_t status = reader.readVarInt();
            const int64_t positionRaw = reader.readLong();
            reader.readInt8(); // face, şimdilik kullanmıyoruz

            if (status != 2) break; // 2 = Finished Digging

            int32_t x, y, z;
            Position::decode(positionRaw, x, y, z);

            const uint8_t brokenType = getBlockType(x, y, z);

            Logger::Info("[Digging] ({}, {}, {}) type={}", x, y, z, static_cast<int>(brokenType));

            m_world.setBlock(x, y, z, 0, 0);
            broadcastBlockChange(x, y, z, 0, 0);

            spawnDroppedItem(x, y, z, droppedItemFor(brokenType));
        }
        catch (const std::exception& e) {
            Logger::Error("[Digging] Parse error: {}", e.what());
        }

        break;
    }

    default:
        break;
    }
}

uint8_t TcpServer::angleToByte(float degrees) {
    return static_cast<uint8_t>(static_cast<int32_t>(degrees * (256.0f / 360.0f)) & 0xFF);
}

void TcpServer::sendSpawnPlayer(SOCKET target, const PlayerInfo& player) {
    PacketWriter writer;
    writer.writeVarInt(player.entityId);
    writer.writeString(player.uuid);
    writer.writeInt(static_cast<int32_t>(player.x * 32.0));
    writer.writeInt(static_cast<int32_t>(player.y * 32.0));
    writer.writeInt(static_cast<int32_t>(player.z * 32.0));
    writer.writeByte(angleToByte(player.yaw));
    writer.writeByte(angleToByte(player.pitch));
    writer.writeShort(0); // Current Item: eli boş
    writer.writeByte(0x7F); // Boş Entity Metadata (liste sonu)

    sendPacket(target, 0x0C, writer);
}

void TcpServer::broadcastSpawnPlayer(const PlayerInfo& player, SOCKET exclude) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);

    for (SOCKET client : m_clients) {
        if (client == exclude) continue;
        sendSpawnPlayer(client, player);
    }
}

void TcpServer::broadcastDestroyEntity(int32_t entityId, SOCKET exclude) {
    PacketWriter writer;
    writer.writeVarInt(1);
    writer.writeVarInt(entityId);

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    for (SOCKET client : m_clients) {
        if (client == exclude) continue;
        sendPacket(client, 0x13, writer);
    }
}

void TcpServer::broadcastEntityTeleport(const PlayerInfo& player, SOCKET exclude) {
    PacketWriter writer;
    writer.writeVarInt(player.entityId);
    writer.writeInt(static_cast<int32_t>(player.x * 32.0));
    writer.writeInt(static_cast<int32_t>(player.y * 32.0));
    writer.writeInt(static_cast<int32_t>(player.z * 32.0));
    writer.writeByte(angleToByte(player.yaw));
    writer.writeByte(angleToByte(player.pitch));
    writer.writeBool(player.onGround);

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    for (SOCKET client : m_clients) {
        if (client == exclude) continue;
        sendPacket(client, 0x18, writer);
    }
}

void TcpServer::handlePlayerPosition(SOCKET clientSocket, double x, double y, double z, bool onGround) {
    PlayerInfo snapshot{};
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        const auto it = m_players.find(clientSocket);
        if (it == m_players.end()) return;

        it->second.x = x;
        it->second.y = y;
        it->second.z = z;
        it->second.onGround = onGround;

        snapshot = it->second;
        found = true;
    }

    if (found) {
        broadcastEntityTeleport(snapshot, clientSocket);
        checkItemPickups(snapshot);
    }
}

void TcpServer::handlePlayerLook(SOCKET clientSocket, float yaw, float pitch, bool onGround) {
    PlayerInfo snapshot{};
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        const auto it = m_players.find(clientSocket);
        if (it == m_players.end()) return;

        it->second.yaw = yaw;
        it->second.pitch = pitch;
        it->second.onGround = onGround;

        snapshot = it->second;
        found = true;
    }

    if (found) broadcastEntityTeleport(snapshot, clientSocket);
}

void TcpServer::handlePlayerPositionAndLook(SOCKET clientSocket, double x, double y, double z, float yaw, float pitch, bool onGround) {
    PlayerInfo snapshot{};
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        const auto it = m_players.find(clientSocket);
        if (it == m_players.end()) return;

        it->second.x = x;
        it->second.y = y;
        it->second.z = z;
        it->second.yaw = yaw;
        it->second.pitch = pitch;
        it->second.onGround = onGround;

        snapshot = it->second;
        found = true;
    }

    if (found) {
        broadcastEntityTeleport(snapshot, clientSocket);
        checkItemPickups(snapshot);
    }
}

void TcpServer::handleBlockPlacement(SOCKET clientSocket, int32_t x, int32_t y, int32_t z, int8_t face, int16_t itemId) {
    if (face == -1) return; // Oyuncu havaya bakiyor, hedef blok yok
    if (itemId <= 0 || itemId > 255) return; // Elde yerlestirilebilir bir blok yok (arac/yiyecek vb.)

    int32_t nx = x;
    int32_t ny = y;
    int32_t nz = z;

    switch (face) {
    case 0: ny -= 1; break; // -Y (Asagi)
    case 1: ny += 1; break; // +Y (Yukari)
    case 2: nz -= 1; break; // -Z (Kuzey)
    case 3: nz += 1; break; // +Z (Guney)
    case 4: nx -= 1; break; // -X (Bati)
    case 5: nx += 1; break; // +X (Dogu)
    default: return;
    }

    if (getBlockType(nx, ny, nz) != 0) return; // Hedef konum zaten dolu

    const uint8_t blockType = static_cast<uint8_t>(itemId);

    Logger::Info("[Placement] ({}, {}, {}) type={}", nx, ny, nz, static_cast<int>(blockType));

    m_world.setBlock(nx, ny, nz, blockType, 0);
    broadcastBlockChange(nx, ny, nz, blockType, 0);
}

uint8_t TcpServer::getBlockType(int32_t x, int32_t y, int32_t z) const {
    BlockData overrideBlock{};
    if (m_world.getOverride(x, y, z, overrideBlock)) return overrideBlock.type;

    // Dunya.cpp'deki sendChunkColumn ile ayni duz dunya jenerasyonu
    if (y == 0) return 7; // Bedrock
    if (y >= 1 && y <= 3) return 1; // Tas
    if (y >= 4 && y <= 6) return 3; // Toprak
    if (y == 7) return 2; // Cimen

    return 0; // Hava
}

void TcpServer::spawnDroppedItem(int32_t x, int32_t y, int32_t z, int16_t itemId) {
    if (itemId < 0) return; // ornegin bedrock -> dusen esya yok

    const int32_t entityId = m_nextEntityId.fetch_add(1);

    const double posX = x + 0.5;
    const double posY = y + 0.25;
    const double posZ = z + 0.5;

    {
        std::lock_guard<std::mutex> lock(m_itemsMutex);
        m_droppedItems[entityId] = DroppedItem{ posX, posY, posZ, itemId };
    }

    PacketWriter spawnWriter;
    spawnWriter.writeVarInt(entityId);
    spawnWriter.writeByte(2); // Object Type: 2 = Item Stack
    spawnWriter.writeInt(static_cast<int32_t>(posX * 32.0));
    spawnWriter.writeInt(static_cast<int32_t>(posY * 32.0));
    spawnWriter.writeInt(static_cast<int32_t>(posZ * 32.0));
    spawnWriter.writeByte(0); // Pitch
    spawnWriter.writeByte(0); // Yaw
    spawnWriter.writeInt(1);  // Data != 0 -> hiz alanlari takip eder
    spawnWriter.writeShort(0);
    spawnWriter.writeShort(0);
    spawnWriter.writeShort(0);

    PacketWriter metadataWriter;
    metadataWriter.writeVarInt(entityId); // Eksikti: Entity Metadata paketi once Entity ID ister
    metadataWriter.writeByte(0xAA); // (type 5 << 5) | index 10 -> Slot metadata
    metadataWriter.writeShort(itemId);
    metadataWriter.writeByte(1); // Count
    metadataWriter.writeShort(0); // Damage/meta
    metadataWriter.writeByte(0); // NBT: TAG_End (yok)
    metadataWriter.writeByte(0x7F); // Metadata listesi sonu

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (SOCKET client : m_clients) {
        sendPacket(client, 0x0E, spawnWriter);
        sendPacket(client, 0x1C, metadataWriter);
    }
}

void TcpServer::checkItemPickups(const PlayerInfo& player) {
    constexpr double PICKUP_RADIUS_SQ = 1.0 * 1.0;

    std::vector<int32_t> collected;

    {
        std::lock_guard<std::mutex> lock(m_itemsMutex);
        for (auto it = m_droppedItems.begin(); it != m_droppedItems.end();) {
            const double dx = it->second.x - player.x;
            const double dy = it->second.y - player.y;
            const double dz = it->second.z - player.z;

            if (dx * dx + dy * dy + dz * dz <= PICKUP_RADIUS_SQ) {
                collected.push_back(it->first);
                it = m_droppedItems.erase(it);
            }
            else ++it;
        }
    }

    for (int32_t itemEntityId : collected) {
        broadcastCollectItem(itemEntityId, player.entityId);
        broadcastDestroyEntity(itemEntityId, INVALID_SOCKET);
    }
}

void TcpServer::broadcastCollectItem(int32_t itemEntityId, int32_t collectorEntityId) {
    PacketWriter writer;
    writer.writeVarInt(itemEntityId);
    writer.writeVarInt(collectorEntityId);

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (SOCKET client : m_clients) sendPacket(client, 0x0D, writer);
}

void TcpServer::broadcastBlockChange(int32_t x, int32_t y, int32_t z, uint8_t type, uint8_t meta) {
    PacketWriter writer;
    writer.writeLong(Position::encode(x, y, z));
    writer.writeVarInt(static_cast<int32_t>((type << 4) | (meta & 0x0F)));

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (SOCKET client : m_clients) sendPacket(client, 0x23, writer);
}

void TcpServer::sendStatusResponse(SOCKET clientSocket) {
    size_t onlinePlayers = 0;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        onlinePlayers = m_clients.size();
    }

    PacketWriter writer;

    const std::string json =
        "{\"version\":{\"name\":\"" + SERVER::VERSION + "\",\"protocol\":" + std::to_string(SERVER::PROTOCOL) + "},"
        "\"players\":{\"max\":" + std::to_string(SERVER::MAX_PLAYER) + ",\"online\":" + std::to_string(onlinePlayers) + "},"
        "\"description\":{\"text\":\"\","
        "\"extra\":["
        "{\"text\":\"" + SERVER::NAME + "\",\"color\":\"aqua\",\"bold\":true},"
        "{\"text\":\" " + SERVER::TAG + "\",\"color\":\"white\",\"bold\":true},"
        "{\"text\":\"\\n\"},"
        "{\"text\":\"⚡ \",\"color\":\"yellow\",\"bold\":true},"
        "{\"text\":\"" + SERVER::DESCRIPTION + "\",\"color\":\"green\"},"
        "{\"text\":\" | \",\"color\":\"dark_gray\"},"
        "{\"text\":\"" + SERVER::VERSION + "\",\"color\":\"gray\"}"
        "]}}";

    writer.writeString(json);



    Logger::Info(
        "[Status] Online Players: {}/{}",
        onlinePlayers,
        SERVER::MAX_PLAYER
    );

    Logger::Info("[Status] Sending Status Response...");

    sendPacket(
        clientSocket,
        0x00,
        writer
    );
}

void TcpServer::sendPong(SOCKET clientSocket, int64_t payload) {
    PacketWriter writer;
    writer.writeLong(payload);

    Logger::Info("[Status] Sending Pong...");

    sendPacket(
        clientSocket,
        0x01,
        writer
    );
}

void TcpServer::shutdown() {
    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }

    WSACleanup();

    m_running = false;
}