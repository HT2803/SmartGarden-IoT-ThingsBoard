# Cấu hình Server (ThingsBoard Cloud) & Logic Tự Động Hóa

## 1. Nền tảng & Kết nối
- **Nền tảng Cloud:** Sử dụng ThingsBoard Cloud chuyên dụng cho hệ thống quản lý IoT.
- **Giao thức dữ liệu cảm biến:** MQTT (Message Queuing Telemetry Transport) giúp tối ưu hóa băng thông đường truyền từ thiết bị phần cứng lên server và cập nhật dữ liệu với độ trễ cực thấp.
- **Xác thực thiết bị:** Mạch ESP32 kết nối bảo mật tới Server thông qua một Access Token duy nhất được cấp phát riêng (`HWNXTu7adTvVtFd5TjO6`).

## 2. Hệ thống Rule Chain & Kịch bản Tự động hóa (Automation Logic)
Để giảm tải tối đa cho máy chủ Đám mây và đảm bảo hệ thống phần cứng vận hành an toàn ngay cả khi mất tín hiệu Internet, hệ thống được cấu hình theo mô hình **Tự động hóa kết hợp giữa Điện toán đám mây (Cloud Computing) và Thiết bị ngoại vi (Edge Computing)**:

### A. Xử lý kịch bản Báo động trên Server (Cloud)
- Hệ thống sử dụng **Root Rule Chain** mặc định tích hợp sẵn của nền tảng ThingsBoard.
- Tích hợp thêm một bộ lọc logic bằng mã JavaScript (`Script Filter Node`) với điều kiện kiểm tra dữ liệu thô: `return msg.soilRaw > 3000;`.
- Khi độ ẩm đất sụt giảm vượt ngưỡng thô 3000 (tương đương với tình trạng đất bị khô khốc), Rule Chain trên Cloud sẽ lập tức kích hoạt Node `Create Alarm` để tạo một **Báo động Đỏ (Critical Alarm)** trực tiếp trên hệ thống ThingsBoard, đồng thời đồng bộ tín hiệu cảnh báo nhấp nháy liên tục về ứng dụng giao diện Web Client.

### B. Xử lý kịch bản Điều khiển Đèn và phân quyền hệ thống (Giao thức kết hợp)
Để đảm bảo tính phản hồi tức thời của thiết bị phần cứng và bảo mật luồng thông tin, hệ thống điều khiển được phân tách mạch lạc:

- **Logic tự động hóa tại chỗ (Local Logic trên ESP32):** Mạch ESP32 liên tục tự đọc cảm biến Ánh sáng (Lux). Khi phát hiện cường độ ánh sáng sụt giảm xuống dưới ngưỡng cài đặt (Lux < 50), chip sẽ tự ra quyết định bật Đèn LED tích hợp. Khi trời sáng trở lại, chip tự động tắt đèn. Trạng thái đèn sau đó được đóng gói JSON gửi ngược lên ThingsBoard để lưu trữ nhật ký thiết bị.
- **Luồng tài khoản Guest (Khách):** Ứng dụng Web Client sử dụng phương thức HTTP GET công khai định kỳ 2 giây/lần để kéo số liệu cảm biến từ ThingsBoard về hiển thị cho người dùng, đồng thời hệ thống tự động khóa chặt quyền tương tác.
- **Luồng tài khoản Admin (Quản trị):** Xác thực an toàn qua mã bảo mật JWT Token. Khi Admin nhấn nút tương tác điều khiển, Web Client sẽ sử dụng giao thức HTTP POST truyền tải gói tin RPC hai chiều (`twoway`) trực tiếp đến địa chỉ endpoint API `/api/plugins/rpc/twoway/{deviceId}` của ThingsBoard để ép mạch chuyển sang chế độ điều khiển bằng tay (Manual Mode) trong 30 giây và thực hiện Bật/Tắt đèn theo đúng yêu cầu từ xa.
