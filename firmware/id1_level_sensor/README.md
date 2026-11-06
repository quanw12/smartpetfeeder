# ID 1 - Đo mức thức ăn (HC-SR04)

Phần của Thuận. ESP32 đo khoảng cách từ nắp phễu xuống mặt thức ăn rồi đổi ra phần trăm. Cứ 5 giây nó gửi kết quả lên `feeder/level` một lần.

*File do AI (Claude) sinh ngày 06/11/2026, Thuận đọc hiểu và chỉnh trước khi nộp.*

## Nối dây

| HC-SR04 | ESP32 | Ghi chú |
|---|---|---|
| VCC | VIN / 5V | HC-SR04 cần 5 V |
| GND | GND | Chung GND với servo SG90 |
| Trig | GPIO 5 | 3,3 V từ ESP32 vẫn kích được |
| Echo | GPIO 18 qua cầu chia áp 1 kΩ / 2,2 kΩ | Echo ra 5 V, chia còn khoảng 3,4 V cho an toàn chân ESP32 |

Cầu chia áp: Echo → 1 kΩ → GPIO 18, và GPIO 18 → 2,2 kΩ → GND.

## Cách đo

- Cứ 200 ms đo một lần, mỗi lần chờ tiếng vọng tối đa 25 ms nên không chặn `loop()`.
- Lần đo không có tiếng vọng hoặc ra ngoài khoảng 2–400 cm được tính là lỗi.
- Lấy **trung vị** của các lần hợp lệ trong 5 lần đo gần nhất, vì một lần sóng dội vào thành phễu sẽ không làm lệch kết quả như khi lấy trung bình.
- Mức (%) = 100 × (khoảng cách lúc rỗng − khoảng cách đo được) / (khoảng cách lúc rỗng − khoảng cách lúc đầy), kẹp trong 0–100.
- Trong 5 lần gần nhất có ít hơn 3 lần hợp lệ thì không gửi gì, chỉ in cảnh báo ra Serial. Vì vậy khi tuột dây, hộp ngừng gửi sau khoảng 1 s chứ không gửi mãi số cũ.

## Dữ liệu gửi đi

Topic `feeder/level`, không retain:

```json
{"level":72,"distance_cm":8.5}
```

Node-RED dùng `level` cho gauge (ID 1), Supabase (ID 3) và ntfy (ID 5). `distance_cm` chỉ để hiệu chỉnh và debug.

## Hiệu chỉnh

1. Nạp code, mở Serial Monitor 115200.
2. Để phễu rỗng, ghi lại `distance_cm`, sửa `KHOANG_CACH_RONG_CM`.
3. Đổ đầy phễu, ghi lại `distance_cm`, sửa `KHOANG_CACH_DAY_CM`.
4. Nạp lại.

Giá trị mặc định là 20 cm lúc rỗng và 4 cm lúc đầy.

## Chạy

1. Copy `secrets.example.h` thành `secrets.h` rồi điền WiFi và MQTT. Dùng cùng broker với ID 2, nhưng nên tạo một user MQTT riêng.
2. Cài thư viện PubSubClient, chọn board "ESP32 Dev Module", rồi nạp code.

**Thử trên Wokwi:** tạo project ESP32 rồi dán `diagram.json`, `libraries.txt` và file `.ino`. Trong `secrets.h`, dùng bộ cấu hình Wokwi ở cuối `secrets.example.h`. Bấm vào cảm biến để kéo thanh khoảng cách. Wokwi không cần cầu chia áp.

**Xem dữ liệu:** subscribe `feeder/level` bằng HiveMQ Web Client hoặc `mosquitto_sub -h broker.hivemq.com -t feeder/level`.

## Gộp với ID 2

Đã gộp ở [`firmware/smart_pet_feeder`](../smart_pet_feeder/README.md). Sản phẩm thật nạp sketch đó, sketch này chỉ để thử riêng cảm biến. Sửa logic đo thì sửa ở cả hai nơi.

## Trạng thái kiểm thử

Đã biên dịch bằng `arduino-cli` với esp32 core 3.3.12. Chưa thử trên Wokwi và chưa nạp lên board thật.
