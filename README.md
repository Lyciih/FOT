
# FOT
Frame OCR Translate - 幀緩存光學辨識翻譯器
![示範畫面1](https://github.com/Lyciih/FOT/blob/main/images/present1.png)

## instruction
一個基於 XCB 幀緩存截圖的 OCR + 翻譯工具

## 使用方法

### 1 clone
```bash
git clone https://github.com/Lyciih/FOT.git
```

### 2 編譯
要編譯跟正確運行fot，你需要先安裝依賴的library跟程式。
- xcb    (x server 的視窗api)
```bash
sudo apt install 
```
- tesseract    (光學辨識library)
```bash
sudo apt install 
```
- translate-shell    (終端機翻譯工具)
```bash
sudo apt install 
```
然後執行
```bash
make
```

### 3 安裝
執行以下命令會把編譯好的fot複製到/usr/local/bin/底下，因此需要sudo。這會讓終端機的自動補全可以找到fot。
```bash
sudo make install
```


### 4 運行一個server
server會接收並顯示光學辨識跟翻譯結果。需要長時間閱讀文件或翻譯時，你可以開啟一個常駐在桌面上。
```bash
fot -s 
```
![示範畫面2](https://github.com/Lyciih/FOT/blob/main/images/present2.png)

可選參數:
```bash
fot -s [server 模式] -ip [ip 地址] -p [port]
```


### 5 運行 client
在另一個終端執行，或是將命令加入自己桌面環境的快捷鍵中。尚未框選任何範圍時，會在桌面範圍顯示一個大的紅叉。
```bash
fot
```
![示範畫面3](https://github.com/Lyciih/FOT/blob/main/images/present3.png)

### 6 框選想要翻譯的詞或句子
框選時，在起始位置按一下即可放開，接著滑鼠可以自由移動，決定好後再按第二下。如果起始位置選錯，可以按esc鍵取消，回到尚未選擇起始位置的狀態，此時若再按一次esc，則會退出client端。你可以連續框選。
![示範畫面4](https://github.com/Lyciih/FOT/blob/main/images/present4.png)

### 7 在server觀看結果
![示範畫面5](https://github.com/Lyciih/FOT/blob/main/images/present5.png)

### 8 按esc鍵退出client端

### 9 按 ctrl + c 鍵關閉server

### 10 解除安裝
```bash
sudo make uninstall
```
