#include "World.hpp"

#include "../Logger/Logger.hpp"
#include "../Protocol/Position.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

World::World(std::string filePath) : m_filePath(std::move(filePath)) {
    try {
        const std::filesystem::path filePath(m_filePath);
        const std::filesystem::path directory = filePath.parent_path();

        if (!directory.empty()) std::filesystem::create_directories(directory);
    }
    catch (const std::filesystem::filesystem_error& e) { Logger::Error("[World] Failed to create directory: {}", e.what()); }
}

World::~World() {
    stopAutoSave();
    save();
}

void World::load() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ifstream file(m_filePath, std::ios::binary);

    if (!file.is_open()) {
        Logger::Info("[World] No existing world file, starting fresh.");
        return;
    }

    int32_t x{}, y{}, z{};
    uint8_t type{}, meta{};
    size_t count = 0;

    while (file.read(reinterpret_cast<char*>(&x), sizeof(x)) &&
        file.read(reinterpret_cast<char*>(&y), sizeof(y)) &&
        file.read(reinterpret_cast<char*>(&z), sizeof(z)) &&
        file.read(reinterpret_cast<char*>(&type), sizeof(type)) &&
        file.read(reinterpret_cast<char*>(&meta), sizeof(meta))) {

        m_overrides[Position::encode(x, y, z)] = BlockData{ type, meta };
        ++count;
    }

    Logger::Info("[World] Loaded {} change records ({} unique blocks) from {}", count, m_overrides.size(), m_filePath);
}

bool World::getOverride(int32_t x, int32_t y, int32_t z, BlockData& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_overrides.find(Position::encode(x, y, z));
    if (it == m_overrides.end()) return false;
    out = it->second;
    return true;
}

void World::setBlock(int32_t x, int32_t y, int32_t z, uint8_t type, uint8_t meta) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_overrides[Position::encode(x, y, z)] = BlockData{ type, meta };
}

void World::save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(m_filePath, std::ios::binary | std::ios::trunc);

    if (!file.is_open()) {
        Logger::Error("[World] Failed to open world file for saving: {}", m_filePath);
        return;
    }

    size_t count = 0;
    for (const auto& [encoded, block] : m_overrides) {
        int32_t x{}, y{}, z{};
        Position::decode(encoded, x, y, z);
        file.write(reinterpret_cast<const char*>(&x), sizeof(x));
        file.write(reinterpret_cast<const char*>(&y), sizeof(y));
        file.write(reinterpret_cast<const char*>(&z), sizeof(z));
        file.write(reinterpret_cast<const char*>(&block.type), sizeof(block.type));
        file.write(reinterpret_cast<const char*>(&block.meta), sizeof(block.meta));
        ++count;
    }

    if (!file) {
        Logger::Error("[World] Failed while writing world file: {}", m_filePath);
        return;
    }

    Logger::Info("[World] Saved {} blocks to {}.", count, m_filePath);
}

void World::startAutoSave() {
    if (m_autoSaveRunning.exchange(true)) return;
    Logger::Info("[World] Auto-save started (10 seconds).");
    m_autoSaveThread = std::thread(&World::autoSaveLoop, this);
}

void World::stopAutoSave() {
    if (!m_autoSaveRunning.exchange(false)) return;
    if (m_autoSaveThread.joinable()) m_autoSaveThread.join();
    Logger::Info("[World] Auto - save stopped.");
}

void World::autoSaveLoop() {
    while (m_autoSaveRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (!m_autoSaveRunning) break;
        save();
    }
}