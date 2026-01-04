// driver.cpp
// clang++ -std=c++17 driver.cpp -o driver
// ./driver
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

int main() {
    // 1. 建立共享記憶體物件
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(CommandRingBuffer));

    // 2. 將共享記憶體映射到目前的進程空間
    auto* rb = (CommandRingBuffer*)mmap(NULL, sizeof(CommandRingBuffer), 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 3. 初始化 (僅由 Driver 初始化一次)
    rb->head.store(0);
    rb->tail.store(0);

    std::cout << "Driver started. Input '1' for DRAW, '2' for CLEAR, '0' to EXIT:\n";

    int input;
    while (std::cin >> input) {
        if (rb->is_full()) {
            std::cout << "Buffer full, waiting for firmware...\n";
            continue;
        }

        uint32_t t = rb->tail.load(std::memory_order_relaxed);
        if (input == 1) rb->buffer[t] = {CMD_DRAW_RECT, 100};
        else if (input == 2) rb->buffer[t] = {CMD_CLEAR_SCREEN, 0};
        else if (input == 0) {
            rb->buffer[t] = {CMD_EXIT, 0};
            rb->tail.store((t + 1) % RING_SIZE, std::memory_order_release);
            break;
        }

        // 重要：使用 memory_order_release 確保資料寫入後才更新 tail
        rb->tail.store((t + 1) % RING_SIZE, std::memory_order_release);
        std::cout << "Command sent.\n";
    }

    munmap(rb, sizeof(CommandRingBuffer));
    return 0;
}