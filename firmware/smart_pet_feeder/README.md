# Firmware gộp ID 1 + ID 2

Đây là chương trình nạp cho sản phẩm thật. Một ESP32 vừa đo mức thức ăn (ID 1, Thuận) vừa điều khiển cửa gạt (ID 2, Quân).

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
| `feeder/level` | ESP32 → Node-RED, 2 phút/lần và ngay sau mỗi lần cho ăn | `{"level":72,"distance_cm":8.5}` |
| `feeder/cmd/feed` | Node-RED → ESP32 | `{"duration_ms":1500}`, không retain |
| `feeder/status` | ESP32 → Node-RED | `opening`, `done`, `rejected` (chi tiết ở README ID 2) |
| `feeder/online` | ESP32 → Node-RED, retained | `{"online":true}`, LWT `{"online":false}` |

## Khi nào gửi mức

- Đo 200 ms/lần, lấy trung vị 5 lần đo gần nhất, gửi `feeder/level` **2 phút một lần**.
- **Ngay sau mỗi lần cho ăn**: khi cửa đóng xong, hộp xoá các lần đo cũ (đo lúc chưa cho ăn), chờ đủ 5 lần đo mới (khoảng 1 s) rồi gửi một lần. Đếm 2 phút bắt đầu lại từ lần gửi này. Nếu 5 lần đo đó không ra được mức hợp lệ (cảm biến lỗi), hộp vẫn chờ và gửi ngay khi cảm biến đo lại được.
- Đang chờ lượt gửi sau cho ăn mà đến hạn 2 phút thì bỏ lượt định kỳ, chỉ gửi một lần khi đủ 5 lần đo mới.
- Cho ăn tiếp trong khoảng 1 s đó thì hộp chỉ gửi một mức, sau lần cuối của cả đợt. Đây là chủ ý: mức giữa các lần chỉ cách nhau dưới 1 s, không có ý nghĩa; mỗi lần cho ăn vẫn được ghi riêng ở ID 3. Không chặn lệnh cho ăn để Dashboard của ID 2 không phải xử lý thêm trường hợp từ chối.
- **Lúc mới bật máy**: gửi ngay khi đủ 5 lần đo và MQTT đã kết nối, để gauge có số sớm. Chờ MQTT tối đa 30 s; không có mạng thì sau 30 s vẫn in ra Serial.
- MQTT rớt đúng lúc gửi thì lần đó bị bỏ, lần kế tiếp là 2 phút sau; nối lại không gửi bù. Kết quả `publish()` chưa được kiểm tra.
- Khi cảm biến và mạng bình thường, Dashboard và cảnh báo ntfy (ID 5) trễ tối đa khoảng 2 phút so với mức thật. Nếu cho ăn liên tục mỗi dưới 1 s thì hộp không gửi mức cho tới khi dừng đợt; cảm biến lỗi thì không gửi cho tới khi đo lại được.

Lý do chọn 2 phút: ID 3 ghi mọi tin mức lên Supabase, và ID 4 đọc tối đa 1000 dòng cho 24 giờ. 2 phút/lần là khoảng 720 bản ghi mức/ngày, mỗi đợt cho ăn thêm một bản ghi mức (và đếm lại 2 phút). ID 4 lấy tối đa 1000 bản ghi mức trong 24 giờ; vượt số này thì phần cũ nhất bị bỏ do chưa phân trang. Bản ghi cho ăn không tính vào giới hạn này. Gửi 5 s/lần thì 17.280 dòng/ngày.

## Khi cảm biến lỗi

Lần đo lỗi cũng được ghi vào 5 lần đo gần nhất. Dưới 3 lần hợp lệ thì hộp không gửi `feeder/level` và in cảnh báo ra Serial. Vì vậy khi tuột dây, hộp ngừng gửi chứ không gửi mãi số cũ. Dashboard vẫn giữ số cuối cùng, xem giờ cập nhật để biết số đó đã cũ. Cửa gạt vẫn hoạt động bình thường.

## Chạy

1. Copy `secrets.example.h` thành `secrets.h` trong thư mục này rồi điền WiFi và MQTT.
2. Cài board **esp32 by Espressif Systems**, thư viện **PubSubClient** và **ESP32Servo** (xem README ID 2).
3. Chọn board **ESP32 Dev Module**, nạp code, mở Serial Monitor **115200**.

**Wokwi:** dán `smart_pet_feeder.ino`, `secrets.h` (bộ cấu hình Wokwi), `diagram.json`, `libraries.txt`. Wokwi không cần cầu chia áp.

## Thử sau khi nối dây

1. Không cần mạng: khoảng 30 s sau khi bật, Serial in `[ID1] -> {...}`, sau đó 2 phút một lần. Gõ `1500` rồi Enter, cửa mở 1,5 s rồi đóng, khoảng 1 s sau Serial in mức mới.
2. Trong lúc cửa mở, cửa vẫn đóng đúng giờ (vòng lặp vẫn đo, không bị chặn).
3. Rút dây Echo: đến lượt gửi kế tiếp, Serial báo không đủ lần đo và không gửi mức. Cắm lại thì lần sau gửi tiếp. Lệnh cho ăn vẫn chạy trong lúc này.
4. Có mạng: bấm CHO ĂN trên Dashboard, xem gauge, bảng lịch sử và trạng thái kết nối.
5. Servo quay mà ESP32 không reset, số đo không nhảy lung tung.

## Hiệu chỉnh khi lắp vào hộp

- **Servo:** cấp nguồn để servo về `GOC_DONG` rồi mới gắn cần servo vào cửa. Chỉnh `GOC_DONG`, `GOC_MO` nếu cửa không khớp.
- **Mức:** đọc `distance_cm` lúc phễu rỗng và lúc đầy, sửa `KHOANG_CACH_RONG_CM`, `KHOANG_CACH_DAY_CM`. Lúc đầy phải cách cảm biến hơn 2 cm.

## Trạng thái kiểm thử

Đã chạy logic bằng `g++` trên máy tính, với WiFi, MQTT, servo và HC-SR04 giả lập (30 kiểm tra đều đạt): đổi ra %, trung vị, ngừng gửi khi tuột dây, mở và đóng cửa đúng giờ, `busy`, `invalid`, kẹp thời gian, đo trong lúc cửa mở, rớt MQTT lúc cửa mở, `millis()` tràn. Bài test này không nằm trong repo.

Đã biên dịch bằng `arduino-cli` cho board ESP32 Dev Module với esp32 core 3.3.12, PubSubClient 2.8 và ESP32Servo 3.2.1, cả khi bật và tắt TLS. Bản bật TLS chiếm khoảng 76% bộ nhớ chương trình và 14% RAM.

**10/11/2026 (đổi sang gửi 2 phút + sau khi cho ăn):** chạy lại logic bằng `g++` với Arduino giả lập: gửi lần đầu sau khoảng 1 s, không gửi trước 2 phút, gửi đúng 2 phút, sau khi cho ăn gửi bằng số đo mới (không lẫn số đo cũ), đếm 2 phút lại từ đó, tuột dây thì không gửi, bật máy chưa có MQTT thì chờ. Cảm biến lỗi ngay sau cho ăn rồi hết lỗi thì vẫn gửi ngay; 3 lệnh cho ăn 200 ms liên tiếp thì có 3 `done` và một mức sau lần cuối; `millis()` tràn không làm chờ MQTT lại 30 s. Bản này **chưa biên dịch lại bằng `arduino-cli`**.

**Chưa làm:** chạy trên Wokwi, nạp lên board. Biên dịch và giả lập không kiểm tra được sụt áp, nhiễu servo hay kết nối TLS thật.
