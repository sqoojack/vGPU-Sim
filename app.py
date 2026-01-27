# python -m streamlit run app.py
import streamlit as st
import mmap
import struct
import numpy as np
import time
import os

# 設定頁面
st.set_page_config(page_title="vGPU Sim", layout="wide")
st.title("🖥️ vGPU Architecture Simulator (macOS/Linux)")

# --- 設定區 ---
# 必須與 common.h 中的定義一致
SHM_FILENAME = "vgpu_ram.bin" 
WIDTH = 640
HEIGHT = 480
VRAM_SIZE = WIDTH * HEIGHT * 4

# GPUState Header format (與 C++ struct GPUState 對齊)
# uint32 magic (4 bytes)
# uint32 running (4 bytes)
# uint32 frame_counter (4 bytes)
# float temperature (4 bytes)
# 總共 16 bytes
HEADER_FMT = "IIIf" 
HEADER_SIZE = struct.calcsize(HEADER_FMT)

def get_data():
    # 檢查檔案是否存在
    if not os.path.exists(SHM_FILENAME):
        return None, None
    
    try:
        # 使用標準檔案開啟模式
        with open(SHM_FILENAME, "r+b") as f:
            # 建立記憶體映射 (唯讀模式即可)
            mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
            
            # 1. 讀取標頭 (Header)
            header_bytes = mm[:HEADER_SIZE]
            header_data = struct.unpack(HEADER_FMT, header_bytes)
            
            # 2. 讀取 VRAM
            # VRAM 緊接著 Header 之後
            vram_offset = HEADER_SIZE
            vram_bytes = mm[vram_offset : vram_offset + VRAM_SIZE]
            
            mm.close()
            return header_data, vram_bytes
    except Exception as e:
        st.error(f"讀取錯誤: {e}")
        return None, None

# 執行讀取
header, vram_bytes = get_data()

if header:
    magic, running, frame, temp = header
    
    # 驗證 Magic Number (確保我們讀到正確的 vGPU 記憶體檔)
    # 0x56475055 = "VGPU" in ASCII
    if magic != 0x56475055:
        st.error(f"記憶體檔案損毀或版本不符 (Magic: {hex(magic)})")
    else:
        col1, col2 = st.columns([3, 1])
        
        with col1:
            st.subheader("VRAM Visualization")
            if vram_bytes:
                raw_img = np.frombuffer(vram_bytes, dtype=np.uint8).reshape((HEIGHT, WIDTH, 4))
                bgr_img = raw_img[:, :, :3]
                st.image(bgr_img, channels="BGR", use_container_width=True)
        
        with col2:
            st.subheader("System Telemetry")
            st.metric("System Status", "Running" if running else "Stopped", 
                     delta="Online" if running else "Offline")
            st.metric("Frame Counter", frame)
            
            # 溫度顯示
            temp_delta = temp - 40.0
            st.metric("GPU Temperature", f"{temp:.1f} °C", 
                     delta=f"{temp_delta:.1f} °C", 
                     delta_color="inverse")
            
            st.info(f"Memory File: {SHM_FILENAME}")
            st.caption(f"VRAM Size: {VRAM_SIZE/1024:.0f} KB")
        
        # 自動刷新 (約 10 FPS)
        time.sleep(0.1)
        st.rerun()

else:
    # 如果找不到檔案
    st.warning(f"找不到韌體記憶體檔案: {SHM_FILENAME}")
    st.info("請先執行: ./firmware")
    
    if st.button("嘗試重新連線"):
        st.rerun()