# Node-RED

## `id2_feed_flow.json` — ID 2: nút CHO ĂN (Quân)

Tạo một trang Dashboard gồm thanh trượt thời gian mở (0,2–5 s, mặc định 1,5 s), nút **CHO ĂN**, dòng trạng thái và dòng kết nối hộp. Bên dưới là các node gửi lệnh và đọc xác nhận qua MQTT, đúng bảng topic trong [README của firmware](../firmware/id2_servo_feed/README.md).

Cần `@flowfuse/node-red-dashboard` (Dashboard 2.0). Flow đã chạy thử với Node-RED 5.0.9 và Dashboard 1.34.1.

### Chạy thử

1. Mở Node-RED: `node-red`, vào <http://localhost:1880>.
2. Menu ☰ → **Import**, chọn `id2_feed_flow.json`, chọn *new flow*, rồi **Deploy**.
3. Mở <http://localhost:1880/dashboard/feeder>.

**Nếu máy đã có Dashboard khác:** flow này mang theo một `ui-base` riêng cùng đường dẫn `/dashboard`. Một Node-RED chỉ nên có một `ui-base`, nên sau khi import hãy mở node `ui-page` "Dashboard", chọn `ui-base` đang có, rồi xoá "Hộp cho ăn" trong tab Configuration nodes.

### Đổi sang HiveMQ Cloud

Mặc định flow dùng `broker.hivemq.com:1883`, khớp với bộ cấu hình Wokwi của firmware. Đây là broker công khai, chỉ dùng để thử. Khi chạy thật, mở node cấu hình **Broker hộp cho ăn**:

- **Server:** `xxxxxxxx.s1.eu.hivemq.cloud`, **Port:** `8883`, bật **Use TLS**.
- Tab **Security:** điền user và pass MQTT.

Node-RED lưu user và pass trong `flows_cred.json`, không nằm trong file flow export nên không lên git. Dùng user MQTT khác với `feeder-esp32` của ESP32.

### Trạng thái hiển thị

| Trạng thái | Nghĩa |
|---|---|
| Đã gửi lệnh, đang chờ hộp... | Node-RED đã gửi lệnh, chưa có phản hồi |
| Đang mở cửa (1.5 s) | ESP32 đã nhận lệnh và mở cửa |
| Đã cho ăn xong (1.5 s) | Cửa đã đóng |
| Cửa đang mở, lệnh mới bị bỏ qua | Bấm thêm khi cửa chưa đóng |
| Không nhận được xác nhận từ hộp | Sau 8 s không có `done`: hộp offline hoặc sai broker |

### Việc gộp với các ID khác

- **ID 3 (Thuận):** thêm một node `mqtt in` cho `feeder/status` với output JSON, lọc `result === "done"` để ghi một lần cho ăn vào Supabase.
- **Dashboard chung:** đặt các widget còn lại vào cùng `ui-page` "Dashboard" để giống wireframe mục 2.2 của proposal.
