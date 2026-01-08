// clang++ -std=c++17 firmware.cpp -o firmware
#include "common.h"
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

const float AMBIENT_TEMP = 40.0f;
const float COOLING_FACTOR = 0.05f;
const float THERMAL_LIMIT = 80.0f; // [Day 3] 溫度最高限制

// [Day 4] ARM64 Inline Assembly 優化範例
// 功能：使用組合語言快速計算陣列的總和 (Checksum)
// 證明點：你懂暫存器 (Registers) 和記憶體存取
uint32_t checksum_arm64(const uint32_t *data, size_t count) {
	uint32_t result = 0;
	if (count == 0) return 0;
	// x0 (input %1) = data pointer
	// x1 (input %2) = count
	// w0 (output %0) = result
	// w3 = temp register
	asm volatile(
			"mov %w[res], #0\n\t"          // 初始化結果，使用編譯器分配的暫存器，而非硬編碼 w0
			"1:\n\t"
			"ldr w3, [%[ptr]], #4\n\t"     // 從指標讀取資料，並自動將指標 +4
			"add %w[res], %w[res], w3\n\t" // 累加
			"subs %[cnt], %[cnt], #1\n\t"  // 剩餘數量 -1
			"b.ne 1b\n\t"                  // 若還有剩下的則繼續
			: [res] "=&r"(result),         // 輸出
			[ptr] "+r"(data),            // 輸入與輸出（指標會變動）
			[cnt] "+r"(count)            // 輸入與輸出（計數會變動）
			: 
			: "w3", "cc", "memory"
	);

	return result;
}

// 輔助函式：原子浮點數加法
void add_temp_atomic(std::atomic<float> &atomic_temp, float delta) {
	float current = atomic_temp.load(std::memory_order_relaxed);
	float desired;
	do {
		desired = current + delta;
	} while (!atomic_temp.compare_exchange_weak(
		current, desired, std::memory_order_release, std::memory_order_acquire));
}

int main() {
	int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
	if (shm_fd == -1) {
		std::cerr << "Error: shm_open failed. Run driver first.\n";
		return 1;
	}

	auto *rb =
		(CommandRingBuffer *)mmap(NULL, sizeof(CommandRingBuffer),
									PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	rb->status.current_temperature.store(40.0f);
	rb->status.heartbeat.store(0); // [Day 3] 初始化心跳

	std::cout << "[Firmware] System Online. Thermal Limit: " << THERMAL_LIMIT
				<< "°C\n";

	while (true) {
		// --- [Day 3] Watchdog Heartbeat 更新 ---
		rb->status.heartbeat.fetch_add(1, std::memory_order_relaxed);

		// --- 物理散熱計算 ---
		float current_t =
			rb->status.current_temperature.load(std::memory_order_relaxed);
		float cooling = COOLING_FACTOR * (current_t - AMBIENT_TEMP);
		add_temp_atomic(rb->status.current_temperature, -cooling);

		// --- [Day 3] Thermal Throttling (過熱降頻) ---
		if (current_t > THERMAL_LIMIT) {
			std::cout << "[WARN] Thermal Throttling Active! Slowing down...\n";
			usleep(500000); // 降速：強制睡 0.5 秒
			continue;	// 跳過後面的指令讀取與處理，直接進入下一次自然散熱循環
		}

		if (rb->is_empty()) {
			usleep(10000); // 閒置時稍微休息，避免 CPU 100%
			continue;
		}

		uint32_t h = rb->head.load(std::memory_order_acquire);
		Command cmd = rb->buffer[h];
		float heat_gain = 0.0f;

		// 指令解析
		if (cmd.type == CMD_DRAW_RECT) {
		// [Day 2] 使用 params 計算熱量
		float area_factor = (cmd.params[2] * cmd.params[3]) / 10000.0f;
		heat_gain = 1.5f + area_factor;
		usleep(150000);
		} else if (cmd.type == CMD_CLEAR_SCREEN) {
		heat_gain = 0.5f;
		usleep(50000);
		}

		else if (cmd.type == CMD_CHECKSUM) {
			// 呼叫 Assembly 優化函式
			// 計算 params[4] 陣列的總和
			uint32_t sum = checksum_arm64(cmd.params, 4);
			std::cout << "[GPU] ASM Checksum: " << sum << " (Verified)\n";
			heat_gain = 0.2f; // 運算也產熱
			usleep(10000);
		}

		else if (cmd.type == CMD_SIMULATE_HANG) {
		// 模擬當機: 進入死迴圈，不再更新 Heartbeat
		std::cout << "[FATAL] Simulating Infinite Loop Hang...\n";
		while (true) {
			usleep(100000);
		}
		} else if (cmd.type == CMD_EXIT) {
		// 修正：退出前先更新 head，確保 Driver 知道指令已處理
		rb->head.store((h + 1) % RING_SIZE, std::memory_order_release);
		std::cout << "[GPU] Shutdown command received.\n";
		break;
		}

		// 應用熱量
		add_temp_atomic(rb->status.current_temperature, heat_gain);

		printf("[GPU] CMD: %d | Temp: %.2f°C %s\n", 
           cmd.type, 
           rb->status.current_temperature.load(),
           (rb->status.current_temperature.load() > THERMAL_LIMIT ? "[THROTTLED]" : ""));

		// 完成指令，更新 head
		rb->head.store((h + 1) % RING_SIZE, std::memory_order_release);
	}

	shm_unlink(SHM_NAME); // 清理共享記憶體
	return 0;
}