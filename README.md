# 🚌 STM32 CAN Bus Network

![Master](https://img.shields.io/badge/Master-STM32H743-03234B?logo=stmicroelectronics&logoColor=white)
![Slaves](https://img.shields.io/badge/Slaves-STM32F407%20%7C%20STM32F429-03234B?logo=stmicroelectronics&logoColor=white)
![CAN](https://img.shields.io/badge/CAN-125%20kbps-455A64)
![GUI](https://img.shields.io/badge/GUI-C%23%20WinForms%20%2B%20ScottPlot-512BD4?logo=dotnet&logoColor=white)

<a id="english"></a>**🇬🇧 English** · [🇻🇳 Tiếng Việt](#tieng-viet)

A 3-node **CAN bus network** built from STM32 boards, with a **C# WinForms monitor** on the PC.
An **STM32H743** master (FDCAN, classic mode) bridges the PC and the bus, an **STM32F407** slave acquires analog data, and an **STM32F429** slave runs **closed-loop DC motor speed control (PID)**. The whole network can be monitored and tuned from the GUI in real time.

---

## ✨ Features

- **Master–slave and peer-to-peer messaging:** the master addresses each slave, and slaves can also talk to each other directly.
- **Hardware arbitration test:** a lower CAN ID always wins, so master commands have priority.
- **CAN sniffer:** the H743 forwards all bus traffic to the PC log with a timestamp, ID and data.
- **Data acquisition (Slave 1):** 3-channel ADC readings streamed to the GUI.
- **Motor control (Slave 2):** 10 kHz soft-PWM H-bridge driver, quadrature encoder on TIM4, low-pass filtered RPM and a **PID speed controller** with gains set from the PC.
- **PID tuning window:** live *Setpoint vs Actual RPM* plot, error / output / integral readouts, step test and **CSV export**.

## 🏗️ System architecture

```text
┌──────────────────────┐   UART 115200    ┌──────────────────────────┐
│ CAN Bus Monitor (PC) │ ◀──────────────▶ │ MASTER · STM32H743       │
│ C# WinForms          │   <CMD_...>      │ FDCAN (classic) 125 kbps │
└──────────────────────┘                  └────────────┬─────────────┘
                                                       │ CAN bus
                         ┌─────────────────────────────┴─────────────────────────────┐
                         ▼                                                           ▼
             ┌────────────────────────┐    0x111 / 0x112 (peer-to-peer)  ┌────────────────────────┐
             │ SLAVE 1 · STM32F407    │ ◀──────────────────────────────▶ │ SLAVE 2 · STM32F429    │
             │ bxCAN · 3-ch ADC       │                                  │ bxCAN · DC motor + PID │
             └────────────────────────┘                                  └────────────────────────┘
```

### CAN IDs

| ID | Direction | Purpose |
| :-- | :-- | :-- |
| `0x010` | Master → Slave 1 | LED toggle, start/stop ADC streaming |
| `0x020` | Master → Slave 2 | LED toggle, PWM forward/reverse/brake/stop, PID gains, PID target |
| `0x111` | Slave 1 → Slave 2 | Peer-to-peer button event |
| `0x112` | Slave 2 → Slave 1 | Peer-to-peer button event |

### PC ↔ Master protocol (UART, ASCII frames)

| From PC | Meaning |
| :-- | :-- |
| `<CMD_PING>` | Connection check |
| `<CMD_START_ADC>` / `<CMD_STOP_ADC>` | Start / stop ADC streaming from Slave 1 |
| `<PWM_FWD_50>` / `<PWM_REV_50>` | Run the motor forward / reverse at 50 % |
| `<PWM_BRAKE>` / `<PWM_STOP>` | Brake / coast |
| `<PID_SET:Kp,Ki,Kd>` / `<PID_TARGET:rpm>` | Set PID gains / target speed |

| From Master | Meaning |
| :-- | :-- |
| `<ADC_3CH:v1,v2,v3>` | 3 ADC voltages from Slave 1 |
| `<MOTOR_SPD:rpm,dir,duty,enc>` | Open-loop motor status from Slave 2 |
| `<PID_DATA:target,actual,error,output>` | Closed-loop PID telemetry |

## 📂 Project structure

```text
STM32-CAN-Network/
├── MCU/
│   ├── Master_H743_Ver_1/   # STM32H743 master: FDCAN + UART bridge + CAN sniffer
│   ├── SLAVE_F407_Ver_2/    # STM32F407 slave: 3-channel ADC acquisition
│   └── SLAVE_F429_Ver_2/    # STM32F429 slave: DC motor, encoder, PID
├── GUI/CANBusMonitor/       # C# WinForms app (Form1: monitor, FormPID: PID tuning)
└── Documents/               # GUI user guide, CAN test plan, H743 board schematic
```

## 🚀 Getting started

1. Wire the three boards through **CAN transceivers** to a common CAN bus (CAN_H, CAN_L, 120 Ω termination at both ends, shared GND).
2. Open each project in `MCU/` with **STM32CubeIDE**, build and flash it to its board.
3. Connect a USB-TTL adapter to the H743: adapter TX → **PB15**, adapter RX → **PB14**, GND → GND.
4. Open `GUI/CANBusMonitor` in **Visual Studio** (.NET, Windows), run it, choose the COM port and press **Connect**.
5. Follow the test scenarios in [`Documents/New_CAN_Test_Plan.md`](Documents/New_CAN_Test_Plan.md) and the [GUI user guide](Documents/GUI_User_Guide.md).

---

<a id="tieng-viet"></a>

## 🇻🇳 Tiếng Việt

[🇬🇧 English](#english) · **🇻🇳 Tiếng Việt**

**Mạng CAN bus** 3 node dùng các board STM32, kèm **phần mềm giám sát C# WinForms** trên máy tính.
Master **STM32H743** (FDCAN, chế độ classic) làm cầu nối giữa máy tính và bus. Slave **STM32F407** thu thập tín hiệu analog. Slave **STM32F429** **điều khiển tốc độ động cơ DC vòng kín (PID)**. Toàn bộ mạng có thể giám sát và chỉnh thông số theo thời gian thực từ phần mềm.

### ✨ Tính năng

- **Giao tiếp master–slave và peer-to-peer:** master gửi lệnh tới từng slave, các slave cũng nói chuyện trực tiếp được với nhau.
- **Kiểm thử phân xử (arbitration):** ID nhỏ hơn luôn thắng, nên lệnh của master được ưu tiên.
- **CAN sniffer:** H743 đẩy toàn bộ lưu lượng trên bus lên máy tính, kèm thời gian, ID và dữ liệu.
- **Thu thập dữ liệu (Slave 1):** đọc ADC 3 kênh và gửi liên tục lên phần mềm.
- **Điều khiển động cơ (Slave 2):** cầu H với PWM mềm 10 kHz, encoder đọc bằng TIM4, RPM có lọc thông thấp, **bộ điều khiển PID tốc độ** với hệ số chỉnh từ máy tính.
- **Cửa sổ chỉnh PID:** biểu đồ *Setpoint và RPM thực tế*, hiển thị sai số / đầu ra / tích phân, thử đáp ứng bậc thang (step test) và **xuất CSV**.

Sơ đồ kiến trúc, bảng CAN ID và giao thức UART: xem phần tiếng Anh ở trên.

### 🚀 Hướng dẫn sử dụng

1. Nối 3 board qua **module CAN transceiver** vào cùng một bus (CAN_H, CAN_L, điện trở 120 Ω ở hai đầu, chung GND).
2. Mở từng project trong `MCU/` bằng **STM32CubeIDE**, build và nạp cho đúng board.
3. Nối USB-TTL với H743: TX của module → **PB15**, RX của module → **PB14**, GND → GND.
4. Mở `GUI/CANBusMonitor` bằng **Visual Studio** (.NET, Windows), chạy, chọn cổng COM rồi bấm **Connect**.
5. Làm theo các kịch bản kiểm thử trong [`Documents/New_CAN_Test_Plan.md`](Documents/New_CAN_Test_Plan.md) và [hướng dẫn sử dụng GUI](Documents/GUI_User_Guide.md).

---

<p align="center">Made by <a href="https://github.com/TuanLinh05">Vu Tuan Linh</a> · HCMUT</p>
