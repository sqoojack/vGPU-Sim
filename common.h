#pragma once
#include <atomic>
#include <cstdint>

#define SHM_NAME "/vgpu_shm"
#define RING_SIZE 64  // 必須是 2 的冪次方

enum CommandType {
    CMD_UNUSED = 0,
    CMD_DRAW_RECT,
    CMD_CLEAR_SCREEN,
    CMD_SIMULATE_HANG, // [Day 3] 模擬當機
    CMD_EXIT
};

struct Command {
    CommandType type;
    uint32_t params[4]; // [Day 2] 絕對保留：指令參數
};

struct SystemStatus {
    std::atomic<float> current_temperature;
    std::atomic<uint64_t> heartbeat; // [Day 3] 絕對保留：心跳計數器
};

struct CommandRingBuffer {
    std::atomic<uint32_t> head; 
    std::atomic<uint32_t> tail;
    Command buffer[RING_SIZE];
    SystemStatus status;

    // 檢查 Buffer 是否已滿
    bool is_full() const {
        uint32_t h = head.load(std::memory_order_relaxed);
        uint32_t t = tail.load(std::memory_order_relaxed);
        return ((t + 1) % RING_SIZE) == h;
    }

    // 檢查 Buffer 是否為空
    bool is_empty() const {
        uint32_t h = head.load(std::memory_order_relaxed);
        uint32_t t = tail.load(std::memory_order_relaxed);
        return h == t;
    }
};