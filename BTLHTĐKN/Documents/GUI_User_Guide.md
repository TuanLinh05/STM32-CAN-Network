# Hướng Dẫn Sử Dụng Giao Diện CAN Bus Monitor (C# WinForms)

Chào mừng bạn đến với công cụ giám sát và điều khiển mạng CAN Bus trung tâm. Phần mềm này giúp bạn biến bo mạch **STM32H743 Master** thành một chiếc USB-CAN Dongle để điều khiển và lắng nghe toàn bộ các Slave trên mạng.

---

## 1. Kết nối ban đầu (Connection Panel)
- **Chuẩn bị phần cứng:** Đảm bảo bạn đã cắm module USB-to-TTL vào máy tính. Chân `TX` của module cắm vào `PB15 (RX)` của H7, và `RX` của module cắm vào `PB14 (TX)` của H7. Đấu chung GND.
- **Trên phần mềm:**
  1. Nhấn nút **Refresh** để phần mềm quét lại các cổng COM đang có trên máy tính.
  2. Bấm vào ô danh sách (ComboBox) và chọn đúng cổng COM của module (Ví dụ: `COM3`, `COM4`).
  3. Nhấn nút **Connect**.
- **Kết quả:** Ngay lập tức, mạch H743 sẽ gửi một lời chào phản hồi báo hiệu kết nối thành công:
```text
====================================
[SYSTEM] STM32H743 MASTER CONNECTED!
[SYSTEM] CAN BUS 125KBPS IS READY.
====================================
```

---

## 2. Giám sát lưu lượng mạng (CAN Sniffer Logger)
Khu vực màn hình đen ở giữa phần mềm đóng vai trò như một bảng log Console liên tục ghi nhận dữ liệu:
- Bất cứ khi nào **F407 (Slave 1)** tự động gửi tin nhắn cho **F429 (Slave 2)**, hay Master gửi đi, mạch H743 sẽ chặn bắt và hiển thị ngay lên màn hình.
- Ví dụ nếu Slave 1 bấm nút gửi lệnh `ID: 0x111`, màn hình sẽ hiện: `[14:30:25] [CAN_RX] ID: 0x111, DATA: 01`
- Tính năng này đặc biệt hữu dụng để tìm ra module nào đang phát rác lên mạng (bị lỗi phần cứng).

---

## 3. Bảng điều khiển cảm biến và cơ cấu chấp hành (Control Panel)

### Điều khiển Slave 1 (Tính năng Đọc ADC)
Dùng để kiểm thử khả năng đọc dữ liệu cảm biến liên tục (Ví dụ: biến trở, cảm biến nhiệt độ).
- Nhấn **Start ADC**: Phần mềm sẽ gửi chuỗi lệnh `<CMD_START_ADC>` xuống H743. H743 sẽ phát lệnh CAN (`ID: 0x030`) ép Slave 1 phải trả dữ liệu cảm biến liên tục về.
- Nhấn **Stop ADC**: Phần mềm gửi lệnh `<CMD_STOP_ADC>`, H743 phát lệnh CAN (`ID: 0x031`) yêu cầu Slave 1 dừng.

### Điều khiển Slave 2 (Tính năng Điều khiển Động cơ/PWM)
Dùng để giả lập hệ thống chân ga (Powertrain) điều tốc mượt mà.
- Kéo thanh trượt **(TrackBar)** để thiết lập giá trị từ `0%` đến `100%`.
- Nhấn **Set PWM**: Phần mềm gửi chuỗi (Ví dụ `<PWM_75>`) xuống H743. H743 sẽ phát lệnh CAN (`ID: 0x040`) kèm byte dữ liệu tương ứng. Slave 2 nhận được sẽ tự động điều chỉnh độ sáng đèn LED công suất hoặc tốc độ mô-tơ theo tỷ lệ tương ứng.

---
*Lưu ý: Bạn có thể mở rộng thêm logic đọc và parse giá trị trả về tại hàm `SerialPort_DataReceived` trong code C# nếu muốn cập nhật thông số lên giao diện đồ họa đẹp mắt hơn.*
