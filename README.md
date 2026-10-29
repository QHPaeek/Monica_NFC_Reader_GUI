# Monica NFC Reader GUI

一款基于 Qt 的 Windows 桌面 GUI 工具，用于连接和管理自定义 NFC 读卡器（Monica 系列），支持各类街机游戏卡片的读取识别、Aime 卡写入、读卡器配置及固件更新。

支持 **USB HID** 与 **串口（UART / USB CDC）** 两种连接方式，两者使用同一套 AimeIO 协议。

---

## 主要功能

| 功能                           | 说明                                                                                                |
| ---------------------------- | ------------------------------------------------------------------------------------------------- |
| **卡片读取**                     | 自动识别 Aime / Banapass / AIC / e-Pass / NESiCA / T-Union / Jubeat / Other FeliCa 等多种游戏卡片，显示 UID 和卡号 |
| **Banapass 解密**              | 支持新款 (明文) 和旧款 (Blowfish 加密) Banapass 卡的卡号解析                                                       |
| **Aime 卡写入**                 | 向空白 MIFARE 卡写入 Aime 卡号与密钥，分两步完成 (Block 2 卡号 + Block 3 密钥区)                                        |
| **高级写卡**                     | 自定义扇区/区块/密钥/数据的直接写入模式，不等待卡片检测                                                                     |
| **读卡器配置 (SEGA/SPICE/NAMCO)** | 可视化设置读卡器 Flash 参数：启用的卡片类型、UART 波特率、LED 亮度等                                                        |
| **固件版本校验**                   | 固件低于 v0.5 自动禁用写卡功能，防止误操作                                                                          |
| **LED 灯光控制**                 | 全局亮度（0–255，随「写入设备」保存）+ R / G / B 三个实时滑条（实时变色，不保存）                                                  |
| **双通道连接**                    | 顶部下拉框实时列出 HID 读卡器与全部串口，VID/PID 匹配的设备自动选中；HID 与 UART 走同一套 AimeIO 命令                                  |
| **固件升级 (双通道)**              | **BOOTLOADER 升级**：v0.8 及以上的合并固件，内置串口协议实现（无需 Python）；**DFU 升级**：任何版本均可（含 v0.7 及以下老固件），走 dfu-util。按固件版本自动选通道，串口设备禁止跳转 DFU                |
| **AIC 双排卡号**                 | AIC 卡同时显示解密得到的正确卡号，以及 IDM 直接转十进制（补零 20 位）的对照值                                                   |
| **读取超时保护**                   | 连接后读取 Flash 若超时（默认 3 s）无响应，判定读卡器死机并弹窗提示，连接状态自动复位                                                  |

### 支持的卡片类型

- **Aime (MIFARE)** — 明文卡号，20 位
- **Banapass New (MIFARE)** — 明文卡号，20 位，3 开头
- **Banapass Old (MIFARE)** — Blowfish 加密卡号，自动解密
- **AIC (FeliCa)** — SPAD 解密卡号；校验 `spad[6]` 是否为合法 BCD 十位（`0x50`–`0x59`，即卡号首位为 `5`），否则回退为 IDM 转换
- **e-Pass (ISO15693)** — 8 字节 UID 转换卡号
- **NESiCA (Ultralight)** — 7 字节 UID，明文卡号
- **T-Union (14443-A)** — 4 字节 UID，明文卡号
- **Jubeat (MIFARE)** — 4 字节 UID 转换卡号
- **Other FeliCa** — 8 字节 IDM 转换卡号

### 连接方式

程序顶部实时扫描并列出所有可用设备，下拉框中同时包含：

- **HID 读卡器** — 例如 `Monica NFC Reader（HID）`
- **串口** — 例如 `COM6 USB-SERIAL CH340`；VID/PID 命中时追加标签：
  - `（Monica Bootloader）` — 自研 BOOTLOADER 的 CDC 串口
  - `（Monica Reader）` — 应用固件的 CDC 串口
  - `（ST DFU）` — DFU 模式

VID/PID 匹配的设备会自动选中，但**仍需手动点「连接」**。

「连接」按选中类型分流：

- 选中 **HID** → 建立 HID 连接，可读写配置、刷卡、写卡
- 选中 **串口** → 先尝试 BOOTLOADER 握手；失败则自动以 AimeIO 协议连接，
  之后读配置、刷卡、写卡、控灯均可正常使用

> 串口通道**不支持跳转 DFU**，DFU 只能通过 USB 进入。

---

## 编译构建

### 环境要求

- **Windows** (x86_64)
- **MSYS2** (MinGW64)
- **Qt 6.x** 静态编译版本 (`mingw-w64-x86_64-qt6-static`)
- **HIDAPI** (`mingw-w64-x86_64-hidapi`)
- **OpenSSL** (`mingw-w64-x86_64-openssl`)

### 安装依赖 (MSYS2)

```bash
pacman -S mingw-w64-x86_64-qt6-static \
          mingw-w64-x86_64-hidapi \
          mingw-w64-x86_64-openssl \
          mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-make
```

> 编译用到 Qt SerialPort 模块（`QT += serialport`）。若静态 Qt 包未自带，
> 额外安装 `mingw-w64-x86_64-qt6-serialport`。

### 编译

```bash
cd CardReader
make
```

产出 `Monica_NFC_Reader_GUI.exe`，为完全静态链接的单文件，无需额外 DLL。

> 在非 MSYS2 终端里编译时，需先把工具链加进 PATH：
> `export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"`

### 关于 `resources.qrc`

`resources.qrc` 把 `dfu-util.exe` 及其依赖 DLL（`libusb-1.0.dll`、`libwinpthread-1.dll`）
一起打包进 exe，使程序成为真正单文件。这几条用的是**绝对路径**
（`C:/msys64/mingw64/bin/...`）。

如果你的环境里没有这些文件，`make` 会报 `RCC: Error in 'resources.qrc'`。
把 `resources.qrc` 里对应的三行删掉即可——程序仍可正常编译，
只是 DFU 通道需要手动放一份 `dfu-util.exe` 到 exe 同目录，或点界面上的下载按钮。

### 清理

```bash
make clean
```

---

## AimeIO 通信协议

本程序通过自定义 AimeIO 协议与 Monica 系列读卡器通信，**同一套命令**可走 USB HID 或串口。

### USB 描述符

- **Usage Page**: 0xAFCA
- **Usage**: 0x02
- **Report ID 4 (IN)**: 64 字节 (读卡器 → PC)
- **Report ID 5 (OUT)**: 32 字节 (PC → 读卡器)

### 数据包结构

所有传输数据首字节必须为 `0xFF`，第二字节为命令码。

**上行 (读卡器→PC, Report ID 4):**

```c
typedef union {
    uint8_t raw_data[64];
    struct {
        uint8_t Start;
        uint8_t cmd;
        union {
            struct {
                uint8_t Card_Type;
                uint8_t Card_RawData[3][16];
            };
            struct {
                uint8_t Reader_Type;
                uint8_t Firmware_Vision;
                uint8_t Flash_Data[16];
            };
            struct {
                uint8_t status;
                uint8_t unused[18];
            };
        };
    };
} AimeIO_Up_t;
```

**下行 (PC→读卡器, Report ID 5):**

```c
typedef union {
    uint8_t raw_data[32];
    struct {
        uint8_t Start;
        uint8_t Cmd;
        union {
            uint8_t Flash_data[16];
            struct {
                uint8_t Write_Sector;
                uint8_t Write_Block;
                uint8_t Card_Key_A[6];
                uint8_t Card_Key_B[6];
                uint8_t Write_Card_data[16];
            };
            struct {
                uint8_t LED_R;
                uint8_t LED_G;
                uint8_t LED_B;
            };
            struct {
                uint8_t LED_Brightness;
            };
        };
    };
} AimeIO_Down_t;
```

### 串口传输

串口（UART / USB CDC）与 HID 使用**完全相同的命令字与字段偏移**，只是去掉 HID 的 Report ID：

| 方向 | 长度 | 内容 |
| --- | --- | --- |
| PC → 设备 | **32 字节** | `[0]=0xFF`、`[1]=cmd`、`[2..31]=payload` |
| 设备 → PC | **63 字节** | `[0]=0xFF`、`[1]=cmd`、`[2..]=payload` |

- 固件侧 `AimeIO_Transmit()` 发送 `raw_data + 1`（共 63 字节），因此入帧长度固定
- 发过任意一条命令后，固件把当前接口锁定为串口、模式切到 `MODE_AIME_IO`，
  之后**卡片检测会自动推送到同一串口**
- 波特率由 Flash 中的 `uart_baud` 决定（0=115200 / 1=38400 / 2=9600）；
  连接时程序会依次尝试三种波特率
- 固件在累计帧错误时会于 115200 ↔ 38400 之间自动切换

### 命令列表

| 命令码  | 名称          | 方向  | 说明            |
| ---- | ----------- | --- | ------------- |
| 0x01 | Find_Card   | ↑   | 读卡器主动上报卡片检测   |
| 0x02 | Write_Card  | ↓↑  | 写入 MIFARE 卡数据 |
| 0x60 | LED         | ↓   | 设置实时 RGB 灯光（不保存）  |
| 0x61 | JUMP2DFU    | ↓↑  | 跳转 ST DFU 固件下载模式（仅 USB） |
| 0x62 | JUMP2BOOT   | ↓↑  | 跳转自研 BOOTLOADER (CDC/UART) |
| 0x63 | LED_Brightness | ↓↑ | 设置全局亮度（0–255），值变化时固件写 Flash |
| 0x70 | Flash_Read  | ↓↑  | 读取读卡器配置       |
| 0x71 | Flash_Write | ↓↑  | 写入读卡器配置       |

### 卡片类型枚举

```
0x00 — 无卡 (Card_None)
0x01 — Aime (MIFARE)
0x02 — Banapass New
0x03 — Banapass Old
0x04 — AIC (FeliCa)
0x05 — e-Pass (ISO15693)
0x06 — NESiCA (Ultralight)
0x07 — T-Union (14443-A)
0x08 — Jubeat (MIFARE)
0x09 — 未知 MIFARE
0x10 — 空白 MIFARE
0x11 — 未知 FeliCa
```

### Flash_Read 返回数据

Reader_Type 读卡器型号：

- `0xA1` Monica OpenSource
- `0xA2` Monica Lite
- `0xA3` Monica Standard
- `0xA4` Monica Pro

Flash_Data 结构（16 字节）：

```c
typedef union {
    uint32_t raw[4];
    struct {
        uint8_t fw_version_lo;    // 固件低字节
        uint8_t fw_version_hi;    // 固件高字节 (显示: v{hi}.{lo})
        uint8_t uart_baud;        // 0=115200, 1=38400, 2=9600
        uint8_t led_brightness;   // LED 亮度 0-255
        uint8_t sega_setting;     // SEGA 模式开关
        uint8_t spice_setting;    // SPICE 模式开关
        uint8_t namco_setting;    // NAMCO 模式开关
        // ... 剩余字节保留
    };
} FlashData;
```

### Write_Card 命令

**下行 (发送):**

| 偏移  | 大小  | 字段           |
| --- | --- | ------------ |
| 0   | 1   | Start (0xFF) |
| 1   | 1   | Cmd (0x02)   |
| 2   | 1   | Sector (扇区号) |
| 3   | 1   | Block (区块号)  |
| 4   | 6   | Key A (6 字节) |
| 10  | 6   | Key B (6 字节) |
| 16  | 16  | 写入数据         |

**上行 (回复):**

Start + Cmd + Status (1 字节)

状态码：

| 值    | 含义              |
| ---- | --------------- |
| 0x00 | 写入成功            |
| 0x01 | 未找到卡片           |
| 0x02 | 卡片不一致（操作过程中被移走） |
| 0x03 | Key A 校验失败      |
| 0x04 | Key B 校验失败      |
| 0x05 | 写入失败            |

---

## 项目结构

```
CardReader/
├── src/
│   ├── main.cpp             # 程序入口
│   ├── mainwindow.h/.cpp    # 主窗口 GUI
│   ├── hidworker.h/.cpp     # HID 通信线程
│   ├── aimeserial.h/.cpp    # 串口 AimeIO 通道 (UART / CDC)
│   ├── bootproto.h/.cpp     # 自研 BOOTLOADER 串口协议 (固件烧录)
│   ├── portscan.h/.cpp      # 串口枚举 + USB VID/PID 探测
│   ├── flashdata.h          # FlashData 数据结构
│   ├── banapasscard.h/.cpp  # Banapass 卡号解析 (Blowfish)
│   ├── accesscode.h/.cpp    # AIC SPAD 解密 + 卡号转换 (移植自固件)
│   └── sbox.h               # S_BOX_INV 逆掩码表
├── docs/
│   └── AimeIO_Protocol.md   # AimeIO 协议原始文档
├── tools/
│   └── hidtest.cpp          # HID 设备测试工具
├── Makefile                 # 构建入口
├── MonicaNFCReader.pro      # qmake 项目文件
└── README.md
```

---

*本 README 完全由 AI 生成。*
