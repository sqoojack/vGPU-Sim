// firmware.cpp
// clang++ -std=c++17 firmware.cpp -o firmware
// ./firmware
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

int main() {
    // 1. 打開現有的共享記憶體
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    auto* rb = (CommandRingBuffer*)mmap(NULL, sizeof(CommandRingBuffer), 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    std::cout << "Firmware (GPU) running and polling for commands...\n";

    bool running = true;
    while (running) {
        if (rb->is_empty()) {
            usleep(100000); // 稍微休息，降低 CPU 佔用
            continue;
        }

        // 重要：使用 memory_order_acquire 確保看到最新的資料內容
        uint32_t h = rb->head.load(std::memory_order_relaxed);
        Command cmd = rb->buffer[h];

        // 模擬執行指令
        if (cmd.type == CMD_DRAW_RECT) std::cout << "[GPU] Executing DRAW_RECT\n";
        else if (cmd.type == CMD_CLEAR_SCREEN) std::cout << "[GPU] Executing CLEAR_SCREEN\n";
        else if (cmd.type == CMD_EXIT) running = false;

        // 更新 head，表示已處理完畢
        rb->head.store((h + 1) % RING_SIZE, std::memory_order_release);
    }

    std::cout << "Firmware shutting down.\n";
    shm_unlink(SHM_NAME); // 清除共享記憶體
    return 0;
}