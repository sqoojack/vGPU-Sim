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

    std::cout << "Commands: [1] DRAW_RECT (Heating), [2] CLEAR_SCREEN (Cooling), [0] EXIT\n";

    int input;
    while (std::cin >> input) {
        if (rb->is_full()) continue;

        uint32_t t = rb->tail.load(std::memory_order_relaxed);
        
        if (input == 1) { // DRAW_RECT
            rb->buffer[t].type = CMD_DRAW_RECT;
            rb->buffer[t].params[0] = rand() % 1920; // X 座標
            rb->buffer[t].params[1] = rand() % 1080; // Y 座標
            rb->buffer[t].params[2] = 100;           // Width
            rb->buffer[t].params[3] = 100;           // Height
        } 
        else if (input == 2) { // CLEAR
            rb->buffer[t].type = CMD_CLEAR_SCREEN;
            rb->buffer[t].params[0] = 0xFF0000; // Red color
        }
        else if (input == 0) { // EXIT (必須保留，以通知 Firmware 關閉)
            rb->buffer[t].type = CMD_EXIT;
            rb->tail.store((t + 1) % RING_SIZE, std::memory_order_release);
            std::cout << "Exit command sent to Firmware.\n";
            break; // Driver 自己也跳出迴圈
        }
        else {
            continue;
        }

        rb->tail.store((t + 1) % RING_SIZE, std::memory_order_release);
        
        // 讀取當前 Firmware 狀態
        std::cout << "Command sent. GPU Temp: " << rb->status.current_temperature.load() << "°C\n";
    }

    munmap(rb, sizeof(CommandRingBuffer));
    return 0;
}