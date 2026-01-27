#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <pthread.h>

// 系統規格
#define SHM_FILENAME "vgpu_ram.bin"
#define WIDTH 640
#define HEIGHT 480
#define MAX_TENANTS 2
#define RING_BUFFER_SIZE 128
#define DMA_BUFFER_SIZE (64 * 64 * 4)

// 指令集
enum CommandType {
    CMD_NOP = 0,
    CMD_CLEAR = 1,      
    CMD_DRAW_RECT = 2,  
    CMD_DMA_TEXTURE = 3,
    CMD_CHECKSUM = 4,   // 新增: Assembly 加法測試
    CMD_HANG = 9        // 新增: 模擬死機 (測試 Watchdog 用)
};

struct Command {
    uint32_t type;
    uint32_t params[5]; 
};

struct TenantContext {
    uint32_t active;
    uint32_t pid;
    Command cmd_buffer[RING_BUFFER_SIZE];
    uint32_t head; 
    uint32_t tail; 
    pthread_mutex_t lock;      
    pthread_cond_t not_full;   
    pthread_cond_t not_empty;  
    uint32_t dma_staging_area[DMA_BUFFER_SIZE / 4]; 
};

#pragma pack(push, 1) // 強制對齊，避免 Padding 問題
struct GPUState {
    uint32_t magic;         // 0x56475055
    uint32_t running;
    uint32_t frame_counter;
    float temperature;
    
    // Watchdog 專用
    uint64_t last_heartbeat; // Firmware 更新此時間戳，Watchdog 檢查它
    uint32_t watchdog_reset_count; // 紀錄被看門狗重置幾次

    uint32_t vram[WIDTH * HEIGHT]; 
    TenantContext tenants[MAX_TENANTS];
};
#pragma pack(pop)

#endif