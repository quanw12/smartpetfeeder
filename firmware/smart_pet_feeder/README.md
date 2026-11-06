# Firmware gộp ID 1 + ID 2

Đây là chương trình nạp cho sản phẩm thật. Một ESP32 vừa đo mức thức ăn (ID 1, Thuận) vừa điều khiển cửa gạt (ID 2, Quân).

*File do AI (Claude) gộp ngày 06/11/2026 từ `firmware/id1_level_sensor` và `firmware/id2_servo_feed`. Nhóm đọc hiểu và chỉnh trước khi nộp.*

Hai thư mục cũ vẫn giữ lại để thử riêng từng phần. Cách đo và lý do thiết kế của từng phần nằm trong README của hai thư mục đó.

## Nối dây

Chân VIN chỉ có một mà cần cấp 5 V cho hai thiết bị, nên nối qua thanh nguồn của breadboard.

| Từ | Đến | Ghi chú |
|---|---|---|
| ESP32 VIN / 5V | Thanh (+) | Có 5 V khi cắm sạc qua micro-USB |
| ESP32 GND | Thanh (−) | GND chung cho cả ba thiết bị |
| HC-SR04 VCC | Thanh (+) | |
| HC-SR04 GND | Thanh (−) | |
| HC-SR04 Trig | GPIO 5 | |
| HC-SR04 Echo | 1 kΩ → GPIO 18 | Echo ra 5 V, không nối thẳng vào ESP32 |
| GPIO 18 | 2,2 kΩ → Thanh (−) | Chia áp còn khoảng 3,4 V |
| SG90 dây đỏ | Thanh (+) | Không dùng chân 3V3 |
| SG90 dây nâu | Thanh (−) | |
| SG90 dây cam | GPIO 13 | |

Nếu ESP32 bị reset mỗi lần servo quay (sụt áp), thử sạc 2 A, cáp ngắn hơn, hoặc thêm tụ 470–1000 µF giữa thanh (+) và (−), đúng chiều cực.

## MQTT

| Topic | Chiều | Payload |
|---|---|---|
| `feeder/level` | ESP32 → Node-RED, 5 s/lần | `{"level":72,"distance_cm":8.5}` |
| `feeder/cmd/feed` | Node-RED → ESP32 | `{"duration_ms":1500}`, không retain |
| `feeder/status` | ESP32 → Node-RED | `opening`, `done`, `rejected` (chi tiết ở README ID 2) |
| `feeder/online` | ESP32 → Node-RED, retained | `{"online":true}`, LWT `{"online":false}` |

## Khi cảm biến lỗi

Lần đo lỗi cũng được ghi vào 5 lần đo gần nhất. Dưới 3 lần hợp lệ thì hộp không gửi `feeder/level` và in cảnh báo ra Serial. Vì vậy khi tuột dây, hộp ngừng gửi chứ không gửi mãi số cũ. Dashboard vẫn giữ số cuối cùng, xem giờ cập nhật để biết số đó đã cũ. Cửa gạt vẫn hoạt động bình thường.

## Chạy

1. Copy `secrets.example.h` thành `secrets.h` trong thư mục này rồi điền WiFi và MQTT.
2. Cài board **esp32 by Espressif Systems**, thư viện **PubSubClient** và **ESP32Servo** (xem README ID 2).
3. Chọn board **ESP32 Dev Module**, nạp code, mở Serial Monitor **115200**.

**Wokwi:** dán `smart_pet_feeder.ino`, `secrets.h` (bộ cấu hình Wokwi), `diagram.json`, `libraries.txt`. Wokwi không cần cầu chia áp.

## Thử sau khi nối dây

1. Không cần mạng: Serial phải in `[ID1] -> {...}` mỗi 5 s. Gõ `1500` rồi Enter, cửa mở 1,5 s rồi đóng.
2. Trong lúc cửa mở, mức vẫn phải được in đều.
3. Rút dây Echo: khoảng 1 s sau, Serial báo không đủ lần đo và không gửi mức nữa. Cắm lại thì gửi tiếp. Lệnh cho ăn vẫn chạy trong lúc này.
4. Có mạng: bấm CHO ĂN trên Dashboard, xem gauge, bảng lịch sử và trạng thái kết nối.
5. Servo quay mà ESP32 không reset, số đo không nhảy lung tung.

## Hiệu chỉnh khi lắp vào hộp

- **Servo:** cấp nguồn để servo về `GOC_DONG` rồi mới gắn cần servo vào cửa. Chỉnh `GOC_DONG`, `GOC_MO` nếu cửa không khớp.
- **Mức:** đọc `distance_cm` lúc phễu rỗng và lúc đầy, sửa `KHOANG_CACH_RONG_CM`, `KHOANG_CACH_DAY_CM`. Lúc đầy phải cách cảm biến hơn 2 cm.

## Trạng thái kiểm thử

Đã chạy logic bằng `g++` trên máy tính, với WiFi, MQTT, servo và HC-SR04 giả lập (30 kiểm tra đều đạt): đổi ra %, trung vị, ngừng gửi khi tuột dây, mở và đóng cửa đúng giờ, `busy`, `invalid`, kẹp thời gian, đo trong lúc cửa mở, rớt MQTT lúc cửa mở, `millis()` tràn. Bài test này không nằm trong repo.

**Chưa làm:** biên dịch với ESP32 core và thư viện thật, chạy trên Wokwi, nạp lên board. Giả lập không kiểm tra được sụt áp, nhiễu servo hay kết nối TLS thật.
