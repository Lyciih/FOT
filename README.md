# FOT
**Frame OCR Translate - 幀緩存光學辨識翻譯器**
![示範畫面1](https://github.com/Lyciih/FOT/blob/main/images/present1.png)

## 工具介紹
雖然現今在瀏覽器上已經有許多插件可以做到滑鼠選取後直接彈出側欄顯示翻譯結果，但是在遇到用瀏覽器開啟的pdf檔時，插件就會失效。而且有時為了鍛鍊英文能力，即使可以複製整段文章去翻譯，但還是可能想要在閱讀時只查詢某幾個單字，這時不停的複製跟貼上會影響閱讀時的體驗。

本工具就是為了解決上述問題而產生，藉由滑鼠框選範圍，截圖交給光學辨識的library，辨識出文字後再串接翻譯工具，並將最後的結果用udp傳遞給server端顯示。這樣做的好處是server端可以常駐開啟，如果有某些字出現頻率很高，可以看到前幾次翻譯的結果，並且client端只需要用簡單的命令或是將命令綁在快捷鍵上，就能夠輕鬆啟動。一邊閱讀當前桌面上看到的任何部份，一邊連續框選，直到想要翻到下一頁或滾動頁面時再退出client端，調整好畫面後再啟動新的client端。

## 使用方法

### 1 clone
```bash
git clone https://github.com/Lyciih/FOT.git
```

### 2 編譯
要編譯跟正確運行fot，你需要先安裝依賴的library跟程式。
- xcb    (x server 的視窗api)
- tesseract    (光學辨識library)
- translate-shell    (終端機翻譯工具)
```bash
sudo apt update
sudo apt install libxcb1-dev libxcb-keysyms1-dev libtesseract-dev libleptonica-dev translate-shell
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

### 4 可用參數
```bash
fot [-s, server模式] [-i, IP, 預設:127.0.0.1] [-p, port, 預設:8080] [-h, 幫助]
```


### 5 運行一個server
server會接收並顯示光學辨識跟翻譯結果。需要長時間閱讀文件或翻譯時，你可以開啟一個常駐在桌面上。
```bash
fot -s 
```
![示範畫面2](https://github.com/Lyciih/FOT/blob/main/images/present2.png)



### 6 運行 client
在另一個終端執行，或是將命令加入自己桌面環境的快捷鍵中。尚未框選任何範圍時，會在桌面範圍顯示一個大的紅叉。
```bash
fot
```
![示範畫面3](https://github.com/Lyciih/FOT/blob/main/images/present3.png)

### 7 框選想要翻譯的詞或句子
框選時，在起始位置按一下即可放開，接著滑鼠可以自由移動，決定好後再按第二下。如果起始位置選錯，可以按esc鍵取消，回到尚未選擇起始位置的狀態，此時若再按一次esc，則會退出client端。你可以連續框選。
![示範畫面4](https://github.com/Lyciih/FOT/blob/main/images/present4.png)

### 8 在server觀看結果
![示範畫面5](https://github.com/Lyciih/FOT/blob/main/images/present5.png)

### 9 按esc鍵退出client端
如果滑鼠座標有小於0的部份也會自動退出，避免焦點移到其他視窗上回不來，導致無法離開client端。

### 10 按 ctrl + c 鍵關閉server

### 11 解除安裝
```bash
sudo make uninstall
```

## 授權
本專案採用 [MIT License](./LICENSE)。  
你可以自由使用、修改與分發本軟體，只需保留原始授權聲明。
