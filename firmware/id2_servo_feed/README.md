# ID 2 — Cho ăn từ xa (servo SG90) · Quân

Firmware ESP32 cho Luồng 2 trong proposal: nhận lệnh qua MQTT, mở cửa gạt đúng `duration_ms` rồi đóng, sau đó gửi xác nhận.

## File

| File | Dùng để |
|---|---|
| `id2_servo_feed.ino` | Sketch chính |
| `secrets.example.h` | Mẫu cấu hình WiFi + MQTT (HiveMQ Cloud, kèm bộ cho Wokwi đang comment). Copy thành `secrets.h` rồi điền |
| `secrets.h` | Cấu hình thật, tự tạo trên máy. **Không đưa lên git** (đã có trong `.gitignore`) |
| `diagram.json`, `libraries.txt` | Mạch và thư viện để dán vào Wokwi |

## Giao tiếp MQTT (thống nhất với Thuận)

| Topic | Chiều | Payload |
|---|---|---|
| `feeder/cmd/feed` | Node-RED → ESP32 | `{"duration_ms":1500}` (gửi riêng số `1500` cũng được). **retain = false** |
| `feeder/status` | ESP32 → Node-RED | `{"event":"feed","result":"opening","duration_ms":1500}` lúc bắt đầu mở |
| | | `{"event":"feed","result":"done","duration_ms":1500}` sau khi cửa đã đóng |
| | | `{"event":"feed","result":"rejected","reason":"busy"}` nếu cửa đang mở mà có lệnh mới |
| | | `{"event":"feed","result":"rejected","reason":"invalid"}` nếu payload sai |
| `feeder/online` | ESP32 → Node-RED (retained) | `{"online":true}`. Khi ESP32 rớt mạng, broker tự gửi `{"online":false}` (LWT) |

- `duration_ms` bị kẹp trong khoảng **200–5000 ms** để tránh xả hết phễu.
- Lệnh cho ăn **không được gửi retained**. Nếu gửi retained, mỗi lần ESP32 kết nối lại broker sẽ gửi lại lệnh cũ và hộp tự cho ăn thêm một lần.
- Khi nhận `feeder/status` ở Node-RED, Thuận (ID 3) lấy sự kiện `done` để ghi một lần cho ăn vào Supabase.

## Nối dây (ESP32 LuaNode32)

| SG90 | ESP32 |
|---|---|
| Dây cam (tín hiệu) | GPIO **13** |
| Dây đỏ (+) | **VIN / 5V**. Board cấp nguồn bằng sạc điện thoại qua micro-USB, nên chân này có 5V |
| Dây nâu (−) | **GND** (chung GND với HC-SR04) |

- Không cấp nguồn servo từ chân 3V3: SG90 cần 5V và lúc khởi động có thể kéo vài trăm mA.
- Nếu ESP32 bị reset mỗi lần servo quay (sụt áp), thay sạc điện thoại 2A, dùng cáp ngắn hơn, hoặc cắm thêm tụ 470–1000 µF giữa 5V và GND trên breadboard.
- Khi lắp cơ cấu cửa thật, chỉnh `GOC_DONG` / `GOC_MO` ở đầu file cho khớp.

## Cài đặt Arduino IDE

1. File → Preferences → *Additional boards manager URLs*: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Boards Manager: cài **esp32 by Espressif Systems**. Chọn board **ESP32 Dev Module**.
3. Library Manager: cài **PubSubClient** (Nick O'Leary) và **ESP32Servo** (Kevin Harrington).
4. Cài driver **CP2102** nếu Windows không nhận cổng COM.
5. Nạp code, mở Serial Monitor **115200**.

## Test

**A. Không cần MQTT:** gõ `1500` vào Serial Monitor rồi Enter. Servo phải mở 1,5 s rồi đóng, Serial in ra `opening` rồi `done`.

**B. Wokwi (khi chưa có hàng):** tạo project ESP32 mới trên wokwi.com rồi dán `id2_servo_feed.ino`, `secrets.h` (dùng bộ cấu hình Wokwi trong `secrets.example.h`), `diagram.json` và `libraries.txt`. Bấm Play, đợi `[MQTT] ... OK`. Sau đó vào <https://www.hivemq.com/demos/websocket-client/>, Connect, publish lên topic `feeder/cmd/feed` payload `{"duration_ms":1500}`, rồi subscribe `feeder/status` để xem xác nhận.
> `broker.hivemq.com` là broker công khai, ai cũng publish được vào cùng topic, nên chỉ dùng để thử.

**C. Chạy thật với HiveMQ Cloud:** copy `secrets.example.h` thành `secrets.h`, điền WiFi và thông tin cluster, đặt `MQTT_USE_TLS 1`. Test lại như B bằng tab *Web Client* của cluster, hoặc bằng node `mqtt out` trong Node-RED.

Các trường hợp cần thử: lệnh bình thường; gửi lệnh thứ hai khi cửa đang mở (phải nhận `busy`); payload `abc` (phải nhận `invalid`); `{"duration_ms":99999}` (cửa chỉ mở 5 s); rút WiFi trong lúc cửa đang mở (cửa vẫn phải đóng đúng giờ).

## Gộp với ID 1 (tuần 8)

Sketch đã tách sẵn `id2Setup()`, `id2Loop()` và `id2XuLyLenh()`. Khi gộp với firmware HC-SR04 của Thuận, chỉ cần dùng chung một client WiFi/MQTT, gọi `id2Setup()` trong `setup()` và `id2Loop()` trong `loop()`, rồi cho callback chuyển topic `feeder/cmd/feed` vào `id2XuLyLenh()`. Code phía Thuận cũng không được dùng `delay()` dài trong `loop()`, nếu không cửa sẽ đóng trễ.
