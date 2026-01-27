先執行
```
g++ firmware.cpp -o firmware -lpthread
g++ driver.cpp -o driver -lpthread
```
進行編譯, 然後執行
```
./firmware
```
啟動firmware
打開Streamlit查看GPU狀態
```
python -m streamlit run app.py
```

開啟driver
```
./driver 0
./driver 1
```