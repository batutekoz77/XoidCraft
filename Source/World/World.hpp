#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

struct BlockData {
    uint8_t type;
    uint8_t meta;
};

class World {
public:
    explicit World(std::string filePath);
    ~World();

    void load();
    void save();

    bool getOverride(int32_t x, int32_t y, int32_t z, BlockData& out) const;
    void setBlock(int32_t x, int32_t y, int32_t z, uint8_t type, uint8_t meta);

    void startAutoSave();
    void stopAutoSave();

private:
    void autoSaveLoop();

private:
    std::string m_filePath;

    mutable std::mutex m_mutex;
    std::unordered_map<int64_t, BlockData> m_overrides;

    std::thread m_autoSaveThread;
    std::atomic<bool> m_autoSaveRunning{ false };
};