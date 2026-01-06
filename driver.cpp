// clang++ -std=c++17 driver.cpp -o driver
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread> // [Day 3]
#include <chrono> // [Day 3]
#include "common.h"

// [Day 3] 看門狗執行緒：監控 Heartbeat
void watchdog_thread_func(CommandRingBuffer* rb, bool* running) {
    uint64_t last_heartbeat = rb->status.heartbeat.load();
    int stuck_seconds = 0;
    bool firmware_online = false; // [修正] 新增旗標：確認 Firmware 是否已上線

    std::cout << "[WATCHDOG] Monitoring thread started. Waiting for Firmware signal...\n";

    while (*running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        uint64_t current_heartbeat = rb->status.heartbeat.load();

        // 如果 Firmware 還沒上線 (heartbeat 還是 0)，就不開始計數
        if (!firmware_online) {
            if (current_heartbeat > 0) {
                firmware_online = true;
                std::cout << "[WATCHDOG] Firmware connected! Monitoring active.\n";
                last_heartbeat = current_heartbeat;
            }
            continue; // Firmware 還沒準備好，下一秒再檢查
        }

        // Firmware 已經上線過，開始正常監控
        if (current_heartbeat == last_heartbeat) {
            stuck_seconds++;
            if (stuck_seconds >= 3) { 
                std::cout << "\n\033[1;31m[WATCHDOG ALERT] Firmware HANG detected! (No heartbeat for 3s)\033[0m\n";
                // 為了避免洗頻，觸發後可以重置計數，或者讓 Driver 進入恢復流程
                stuck_seconds = 0; 
            }
        } else {
            stuck_seconds = 0;
            last_heartbeat = current_heartbeat;
        }
    }
}

int main() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(CommandRingBuffer));
    auto* rb = (CommandRingBuffer*)mmap(NULL, sizeof(CommandRingBuffer), 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    rb->head.store(0);
    rb->tail.store(0);
    rb->status.heartbeat.store(0); // 加上這一行, 強制歸零 -> 清除舊的髒數據
    rb->status.current_temperature.store(40.0f); // 溫度也重置

    // [Day 3] 啟動 Watchdog
    bool app_running = true;
    std::thread wd_thread(watchdog_thread_func, rb, &app_running);

    std::cout << "Commands: [1] DRAW (Heat), [2] CLEAR, [9] HANG (Test Watchdog), [0] EXIT\n";

    int input;
    while (std::cin >> input) {
        if (rb->is_full()) {
            std::cout << "Buffer full...\n";
            continue;
        }

        uint32_t t = rb->tail.load(std::memory_order_relaxed);
        
        if (input == 1) { 
            rb->buffer[t].type = CMD_DRAW_RECT;
            // [Day 2] 必須設定參數
            rb->buffer[t].params[2] = 100; 
            rb->buffer[t].params[3] = 100;
        } 
        else if (input == 2) { 
            rb->buffer[t].type = CMD_CLEAR_SCREEN;
            rb->buffer[t].params[0] = 0xFF0000;
        }
        else if (input == 9) { // [Day 3] 測試看門狗
            rb->buffer[t].type = CMD_SIMULATE_HANG;
            std::cout << "[Host] Sending HANG command...\n";
        }
        else if (input == 0) {
            rb->buffer[t].type = CMD_EXIT;
            rb->tail.store((t + 1) % RING_SIZE, std::memory_order_release);
            app_running = false; // 停止 Watchdog
            std::cout << "Exit command sent to Firmware.\n";
            break;
        }
        else {
            continue;
        }

        rb->tail.store((t + 1) % RING_SIZE, std::memory_order_release);
        
        // Telemetry 顯示
        printf("[Host] GPU Temp: %.2f°C\n", rb->status.current_temperature.load());
    }

    if (wd_thread.joinable()) wd_thread.join();
    munmap(rb, sizeof(CommandRingBuffer));
    return 0;
}