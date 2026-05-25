# Cấu hình Server (ThingsBoard Cloud) & Logic Tự Động Hóa

## 1. Nền tảng & Kết nối
- **Nền tảng Cloud:** Sử dụng ThingsBoard Cloud chuyên dụng cho IoT.
- **Giao thức truyền thông:** MQTT (Message Queuing Telemetry Transport) giúp tối ưu hóa băng thông đường truyền và cập nhật dữ liệu thời gian thực (Realtime).
- **Xác thực thiết bị:** Mạch ESP32 kết nối bảo mật tới Server thông qua một Access Token duy nhất được cấp phát riêng.

## 2. Hệ thống Rule Chain & Kịch bản Tự động hóa (Automation Logic)
Để giảm tải cho Server và đảm bảo hệ thống vận hành an toàn ngay cả khi mất kết nối Internet, hệ thống được cấu hình theo mô hình **Tự động hóa kết hợp giữa Điện toán đám mây (Cloud) và Thiết bị ngoại vi (Edge Computing)**:

### A. Xử lý kịch bản Báo động trên Server (Cloud)
- Hệ thống sử dụng **Root Rule Chain** mặc định của ThingsBoard.
- Tích hợp thêm một bộ lọc logic bằng mã JavaScript (`Script Filter Node`) với điều kiện: `return msg.soilRaw > 3000;`.
- Khi độ ẩm đất vượt ngưỡng thô 3000 (tương đương đất bị khô khốc), Rule Chain sẽ lập tức kích hoạt Node `Create Alarm` để tạo một **Báo động Đỏ (Critical Alarm)** trực tiếp trên Dashboard của ThingsBoard và đồng bộ tín hiệu cảnh báo nhấp nháy về ứng dụng Web Client.

### B. Xử lý kịch bản Điều khiển Đèn tự động (Local Automation trên ESP32)
- Để đảm bảo tính độc lập và phản hồi tức thời của phần cứng, **Logic điều khiển Đèn/Máy bơm được lập trình tự động hóa hoàn toàn tại chỗ (Local Logic)** ngay trong mã nguồn của chip ESP32 (Thư mục `device/`).
- **Cơ chế hoạt động:** Chip ESP32 sẽ liên tục đọc cảm biến Ánh sáng (Lux). Khi phát hiện trời tối hoặc cường độ ánh sáng sụt giảm xuống dưới ngưỡng cài đặt (Lux < 50), chip sẽ tự ra quyết định bật Đèn LED. Khi trời sáng, chip tự động tắt đèn.
- Trạng thái Đèn (`ledState = ON/OFF`) sau khi chip tự xử lý sẽ được đóng gói vào chuỗi JSON telemetry và **gửi ngược lên ThingsBoard** để lưu trữ (Save Timeseries) và đồng bộ trạng thái trực quan ra ngoài Web Client.
