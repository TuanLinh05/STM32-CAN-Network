# KẾ HOẠCH KIỂM THỬ HỆ THỐNG MẠNG CAN BUS (3-NODE)
**Phiên bản: 2.0 (Cập nhật theo thực tế phần cứng)**

## TỔNG QUAN HỆ THỐNG
1. **Master Node**: STM32H743 DevEBox (FDCAN - Classic Mode)
2. **Slave 1 Node**: STM32F407 Discovery (bxCAN)
3. **Slave 2 Node**: STM32F429 Discovery (bxCAN)
4. **Giao diện PC**: WinForms app (Kết nối với Master qua USB CDC)

---

## PHẦN 1: KIỂM THỬ CƠ BẢN LỚP VẬT LÝ (ĐÃ TRIỂN KHAI CODE)

### Kịch bản 1: Master điều khiển độc lập các Slave (Mô hình Master-Slave)
- **Hành động**: Nhấn nút `K1` (PE3) trên Master.
  - **Kết quả mong muốn**: Master gửi gói tin `ID: 0x010`. Slave 1 nhận được và đảo trạng thái **LED Cam (PD13)**. Slave 2 không phản ứng.
- **Hành động**: Nhấn nút `K2` (PC5) trên Master.
  - **Kết quả mong muốn**: Master gửi gói tin `ID: 0x020`. Slave 2 nhận được và đảo trạng thái **LED Đỏ (PG14)**. Slave 1 không phản ứng.
- **Mục đích**: Kiểm tra khả năng gửi lệnh điều khiển chính xác theo địa chỉ định tuyến, tính ổn định của mạch phần cứng và thuật toán chống rung 50ms trên H7.

### Kịch bản 2: Giao tiếp chéo giữa hai Slave (Mô hình Peer-to-Peer)
- **Hành động**: Nhấn nút `USER_BTN` (PA0) trên Slave 1 (F407).
  - **Kết quả mong muốn**: Slave 1 gửi gói tin `ID: 0x111`. Slave 2 nhận được và đảo trạng thái **LED Xanh lá (PG13)**. Master có thể "nghe lén" được nếu cấu hình.
- **Hành động**: Nhấn nút `USER_BTN` (PA0) trên Slave 2 (F429).
  - **Kết quả mong muốn**: Slave 2 gửi gói tin `ID: 0x112`. Slave 1 nhận được và đảo trạng thái **LED Xanh lá (PD12)**.
- **Mục đích**: Khẳng định sự ưu việt của CAN Bus: Các Slave có quyền giao tiếp trực tiếp với nhau theo thời gian thực mà không cần thông qua sự điều phối rườm rà của Master.

### Kịch bản 3: Xử lý tranh chấp (Arbitration Test)
- **Hành động**: Nhấn và giữ đồng thời nút `K1` trên Master và nút `PA0` trên Slave 1.
  - **Kết quả mong muốn**: Theo nguyên lý CSMA/CR của CAN Bus, Master (`ID: 0x010`) có độ ưu tiên cao hơn Slave 1 (`ID: 0x111`). Lệnh của Master sẽ giành quyền truyền thành công trước. LED Cam trên Slave 1 sẽ chớp trước, sau đó LED Xanh lá trên Slave 2 mới chớp (sau khi Slave 1 truyền lại gói tin thành công).
- **Mục đích**: Đảm bảo cơ chế phân xử ưu tiên phần cứng của CAN hoạt động chuẩn xác trong trường hợp nghẽn mạng. Master luôn là thiết bị tối cao.

---

## PHẦN 2: KIỂM THỬ NÂNG CAO VỚI PC (ĐỊNH HƯỚNG PHÁT TRIỂN TIẾP THEO)

### Kịch bản 4: Giám sát toàn mạng thời gian thực (CAN Sniffer)
- **Hành động**: Cắm cáp chuyển đổi USB-to-TTL nối vào cổng USART của Master lên máy tính, mở giao diện WinForms.
- **Kết quả mong muốn**: Master H743 đóng vai trò là một CAN Sniffer. Nó thu thập mọi lưu lượng đi qua Bus (Từ Slave 1 gửi Slave 2, hay Master gửi đi...) và đẩy dữ liệu qua cổng USART lên phần mềm WinForms để hiển thị một luồng log chi tiết (Thời gian, ID, Data, DLC).
- **Mục đích**: Có công cụ chuyên nghiệp để chẩn đoán lỗi hệ thống (Network Diagnostic) thay vì chỉ nhìn đèn LED.

### Kịch bản 5: Thu thập dữ liệu cảm biến (Data Acquisition)
- **Hành động**: Nhấn nút "Start Read ADC" trên WinForms.
- **Kết quả mong muốn**: 
  1. WinForms gửi lệnh qua cổng COM (USART) xuống Master.
  2. Master dịch lệnh, đóng gói vào frame CAN gửi đến Slave 1.
  3. Slave 1 khởi động bộ biến đổi ADC, liên tục gửi chuỗi dữ liệu điện áp (hoặc nhiệt độ từ cảm biến nội) về Master qua CAN (Mỗi 50ms).
  4. Master bơm dữ liệu qua USART lên phần mềm để vẽ biểu đồ Real-time (Chart).
- **Mục đích**: Đo lường độ trễ (Latency) và sức chịu tải của đường truyền CAN ở tốc độ 125 kbps.

### Kịch bản 6: Điều khiển cơ cấu chấp hành (PWM Actuator)
- **Hành động**: Trên WinForms, kéo thanh trượt (Slider) thay đổi giá trị từ 0% đến 100%.
- **Kết quả mong muốn**: 
  1. WinForms gửi giá trị phần trăm qua USART xuống Master.
  2. Master dịch lệnh và truyền qua mạng CAN xuống Slave 2.
  3. Slave 2 xuất xung PWM trên cấu hình Timer tương ứng để điều khiển độ sáng của một đèn LED công suất hoặc tốc độ Động cơ DC.
  4. Slave 2 gửi trả thông báo (ACK) về WinForms xác nhận.
- **Mục đích**: Mô phỏng hệ thống điều khiển chân ga/động cơ thực tế trong xe hơi (CAN Powertrain).
