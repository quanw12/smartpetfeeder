# Node-RED

`flows_quan.json` chứa toàn bộ phần của Quân, mỗi chức năng một tab, dùng chung một trang Dashboard và một broker MQTT.

| Tab | Chức năng | Cần gì để chạy thật |
|---|---|---|
| ID 2 - Cho ăn | Thanh trượt, nút CHO ĂN, trạng thái, kết nối hộp | Broker MQTT, ESP32 |
| ID 4 - Biểu đồ 24 giờ | Đọc Supabase mỗi 60 giây, vẽ đường | Supabase có dữ liệu từ ID 3 |
| ID 5 - Thông báo ntfy | Mức dưới 20% thì báo về điện thoại, có công tắc | `feeder/level` từ ID 1, app ntfy |
| ID 8 - Chat Gemini | Hỏi đáp dựa trên dữ liệu Supabase | Supabase, key Gemini |

Cần `@flowfuse/node-red-dashboard` (Dashboard 2.0). Đã chạy với Node-RED 5.0.9 và Dashboard 1.34.1.

## Chạy

1. Copy `.env.example` thành `.env` rồi điền. Thiếu mục nào thì chỉ chức năng đó không chạy.
2. Chạy:

   ```
   powershell -ExecutionPolicy Bypass -File node-red\start.ps1
   ```

3. Mở <http://localhost:1880>, menu ☰ → **Import**, chọn `flows_quan.json`, rồi **Deploy**.
4. Mở <http://localhost:1880/dashboard/feeder>.

**Nếu máy đã có Dashboard khác:** flow này mang theo một `ui-base` riêng cùng đường dẫn `/dashboard`. Một Node-RED chỉ nên có một `ui-base`, nên sau khi import hãy mở node `ui-page` "Dashboard", chọn `ui-base` đang có, rồi xoá "Hộp cho ăn" trong tab Configuration nodes. `ui-base` đang dùng phải có `ui-template` trong *Accept Client Data*, để câu trả lời chat chỉ gửi về đúng người hỏi.

## Việc cần tự làm một lần

Các bước này cần tài khoản của bạn nên phải tự làm:

- **HiveMQ Cloud:** tạo cluster free, tạo hai user MQTT (một cho ESP32, một cho Node-RED). Mở node cấu hình **Broker hộp cho ăn**, đặt **Server** `xxxxxxxx.s1.eu.hivemq.cloud`, **Port** `8883`, bật **Use TLS**, điền user và pass ở tab **Security**. Mặc định flow dùng `broker.hivemq.com` (công khai, ai cũng gửi lệnh được), chỉ dùng để thử.
- **ntfy:** cài app ntfy trên điện thoại, subscribe đúng tên `NTFY_TOPIC`.
- **Gemini:** lấy key tại <https://aistudio.google.com>. Nếu chat báo "đang quá tải" và log có mã 429 thì đổi `GEMINI_MODEL` sang model có gói miễn phí.
- **Supabase:** cùng Thuận tạo project và chạy [`supabase/schema.sql`](../supabase/schema.sql).

## Phần cần chốt với Thuận

ID 4 và ID 8 đọc bảng `feeder_events` theo đề xuất trong `supabase/schema.sql`. ID 3 của Thuận phải ghi đúng cấu trúc này:

| Sự kiện | Khi nào ghi | Dữ liệu |
|---|---|---|
| Đo mức | Mỗi tin `feeder/level` | `{"event":"level","level":72}` |
| Cho ăn | Tin `feeder/status` có `result === "done"` | `{"event":"feed","duration_ms":1500}` |

Nếu Thuận đổi tên bảng hoặc cột thì sửa ba function: "Tạo truy vấn 24 giờ" (ID 4), "Nhận câu hỏi" và "Lưu mức, hỏi lịch sử" (ID 8).

Khi gộp, Thuận đặt gauge, bảng lịch sử, email, đăng nhập vào cùng `ui-page` "Dashboard" để giống wireframe mục 2.2 của proposal.

## Hành vi cần biết khi vấn đáp

**ID 2**

| Trạng thái | Nghĩa |
|---|---|
| Đã gửi lệnh, đang chờ hộp... | Node-RED đã gửi lệnh, chưa có phản hồi |
| Đang mở cửa (1.5 s) | ESP32 đã nhận lệnh và mở cửa |
| Đã cho ăn xong (1.5 s) | Cửa đã đóng |
| Cửa đang mở, lệnh mới bị bỏ qua | Bấm thêm khi cửa chưa đóng |
| Không nhận được xác nhận từ hộp | Sau 8 s không có `done`: hộp offline hoặc sai broker |

**ID 4:** truy vấn lấy mới nhất trước rồi đảo lại. Supabase giới hạn 1000 dòng mỗi lần, nếu sắp tăng dần thì phần mới nhất sẽ bị cắt. Lỗi mạng thì giữ nguyên biểu đồ cũ.

**ID 5:** chỉ báo một lần mỗi đợt sắp hết. Mức phải lên lại từ 25% mới báo tiếp, nếu không số đo dao động quanh 20% sẽ gửi liên tục. Gửi lỗi thì lần đo sau thử lại. Công tắc tắt thì không gửi; bật lại mà mức vẫn thấp thì lần đo kế tiếp sẽ báo. Trạng thái công tắc mất khi khởi động lại Node-RED và quay về bật.

**ID 8:** Node-RED đọc mức mới nhất và lịch sử cho ăn từ Supabase, rồi gửi kèm câu hỏi cho Gemini. Không đọc được Supabase thì báo lỗi luôn, không hỏi Gemini, để nó không tự đoán số liệu. Chatbot chỉ trả lời, không điều khiển được hộp.

## Bảo mật

Repo đang public. Key chỉ nằm trong `.env`, user và pass MQTT nằm trong `flows_cred.json`; cả hai đều không lên git. Trước khi commit flow export lại, kiểm tra không có key trong file.

## Trạng thái kiểm thử

Đã chạy Node-RED thật, bấm trên Dashboard thật, với Supabase, ntfy, Gemini **giả lập cục bộ**. ntfy.sh thật đã nhận đúng định dạng tin nhắn.

**Chưa thử với dịch vụ thật:** Supabase, Gemini (kể cả tên model và gói miễn phí), HiveMQ Cloud, và luồng ID 4, 5, 8 với dữ liệu thật từ ESP32.
