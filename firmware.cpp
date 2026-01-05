// firmware.cpp
// clang++ -std=c++17 firmware.cpp -o firmware
// ./firmware
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

const float AMBIENT_TEMP = 40.0f;
const float COOLING_FACTOR = 0.02f; // 散熱係數

void add_temp_atomic(std::atomic<float>& atomic_temp, float delta) {
    float current = atomic_temp.load(std::memory_order_relaxed);
    float desired;
    do {desired = current + delta;} 
    while (!atomic_temp.compare_exchange_weak(current, desired, 
            std::memory_order_release, 
            std::memory_order_acquire));
};

int main() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    auto* rb = (CommandRingBuffer*)mmap(NULL, sizeof(CommandRingBuffer), 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 初始化狀態
    rb->status.current_temperature.store(40.0f); // 初始室溫 40 度

    while (true) {
        // 1. 物理散熱計算 (每循環一次計算一次)
        float current_t = rb->status.current_temperature.load(std::memory_order_relaxed);
        float cooling = COOLING_FACTOR * (current_t - AMBIENT_TEMP);
        add_temp_atomic(rb->status.current_temperature, -cooling);

        // 2. 更新 Heartbeat (Day 3)
        rb->status.heartbeat.fetch_add(1, std::memory_order_relaxed);

        if (rb->is_empty()) {
            usleep(10000); // 降低閒置時的 CPU 消耗 (100Hz 輪詢)
            continue;
        }

        uint32_t h = rb->head.load(std::memory_order_acquire); // 改用 acquire
        Command cmd = rb->buffer[h];
        float heat_gain = 0.0f;

        if (cmd.type == CMD_DRAW_RECT) {
            float area_factor = (cmd.params[2] * cmd.params[3]) / 10000.0f;
            heat_gain = 1.5f + area_factor;
            usleep(150000);
        } 
        else if (cmd.type == CMD_CLEAR_SCREEN) {
            heat_gain = 0.5f;
            usleep(50000);
        }
        else if (cmd.type == CMD_EXIT) {
            // 修正：退出前必須更新 head
            rb->head.store((h + 1) % RING_SIZE, std::memory_order_release);
            std::cout << "[GPU] Orderly shutdown completed.\n";
            break; 
        }

        add_temp_atomic(rb->status.current_temperature, heat_gain);
        printf("[GPU] CMD: %d | Temp: %.2f°C\n", cmd.type, rb->status.current_temperature.load());

        rb->head.store((h + 1) % RING_SIZE, std::memory_order_release);
    }

    shm_unlink(SHM_NAME);
    return 0;
}