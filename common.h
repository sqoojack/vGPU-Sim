// common.h
#pragma once
#include <atomic>
#include <cstdint>

#define SHM_NAME "/vgpu_shm"
#define RING_SIZE 64  // 必須是 2 的冪次方，方便用 & (SIZE-1) 取代取餘數

enum CommandType {
    CMD_UNUSED = 0,
    CMD_DRAW_RECT,
    CMD_CLEAR_SCREEN,
    CMD_EXIT
};

struct Command {
    CommandType type;
    uint32_t params[4];
};

// 用於模擬硬體狀態的結構
struct SystemStatus {
    std::atomic<float> current_temperature;
    std::atomic<uint64_t> heartbeat; // Day 3 Watchdog 會用到
};

struct CommandRingBuffer {
    // 使用 std::atomic 確保多進程間的原子操作
    // driver 更新 tail，firmware 更新 head
    std::atomic<uint32_t> head; 
    std::atomic<uint32_t> tail;
    Command buffer[RING_SIZE];
    SystemStatus status;

    // 檢查 Buffer 是否已滿
    bool is_full() const {
        uint32_t h = head.load(std::memory_order_relaxed);
        uint32_t t = tail.load(std::memory_order_relaxed);
        return ((t + 1) % RING_SIZE) == h;   // tail + 1 再取RING_SIZE的餘數 是否等於 head
    }

    // 檢查 Buffer 是否為空
    bool is_empty() const {
        uint32_t h = head.load(std::memory_order_relaxed);
        uint32_t t = tail.load(std::memory_order_relaxed);
        return h == t;
    }
};