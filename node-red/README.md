# Node-RED

`flows_quan.json` chứa toàn bộ phần của Quân, `flows_thuan.json` chứa phần của Thuận. Mỗi chức năng là một tab, tất cả dùng chung một trang Dashboard và một broker MQTT. `flows_thuan.json` dùng lại các node cấu hình (broker, ui-base, trang Dashboard) nằm trong `flows_quan.json`, nên phải import `flows_quan.json` trước.

| Tab | Chức năng | Cần gì để chạy thật |
|---|---|---|
| ID 1 - Đo mức | Gauge mức thức ăn, giờ cập nhật | Broker MQTT, ESP32 chạy `firmware/smart_pet_feeder` |
| ID 2 - Cho ăn | Thanh trượt, nút CHO ĂN, trạng thái, kết nối hộp | Broker MQTT, ESP32 chạy `firmware/smart_pet_feeder` |
| ID 3 - Lưu Supabase | Ghi mức (2 phút/lần) và lần cho ăn, bảng 10 sự kiện gần nhất | Supabase |
| ID 4 - Biểu đồ 24 giờ | Đọc Supabase mỗi 60 giây, vẽ đường | Supabase có dữ liệu từ ID 3 |
| ID 5 - Thông báo ntfy | Mức dưới 20% thì báo về điện thoại, có công tắc | `feeder/level` từ ID 1, app ntfy |
| ID 6 - Email 20:00 | Email báo cáo lúc 20:00, có công tắc và nút gửi thử | Supabase, Gmail App Password, `EMAIL_TO` |
| ID 7 - Đăng nhập | Trang đăng ký, đăng nhập, đăng xuất; user lưu trong SQLite | `node-red-node-sqlite` |
| ID 8 - Chat Gemini | Hỏi đáp dựa trên dữ liệu Supabase | Supabase, key Gemini |

Cần cài (menu ☰ → Manage palette → Install): `@flowfuse/node-red-dashboard` (Dashboard 2.0), `node-red-node-email` (ID 6), `node-red-node-sqlite` (ID 7). Đã chạy với Node-RED 5.0.9, Dashboard 1.34.1, node-red-node-sqlite 2.0.1.

## Chạy

1. Copy `.env.example` thành `.env` rồi điền. Thiếu mục nào thì chỉ chức năng đó không chạy.
2. Chạy:

   ```
   powershell -ExecutionPolicy Bypass -File node-red\start.ps1
   ```

3. Mở <http://localhost:1880>, menu ☰ → **Import**, chọn `flows_quan.json`, **Import**; làm lại với `flows_thuan.json`, rồi **Deploy**.
4. Mở <http://localhost:1880/dashboard/feeder>. Chưa đăng nhập thì trang tự chuyển sang `/dashboard/login`; đăng ký một tài khoản ở khung "Đăng ký mới" rồi đăng nhập.

**Nếu máy đã có Dashboard khác:** flow này mang theo một `ui-base` riêng cùng đường dẫn `/dashboard`. Một Node-RED chỉ nên có một `ui-base`, nên sau khi import hãy mở node `ui-page` "Dashboard", chọn `ui-base` đang có, rồi xoá "Hộp cho ăn" trong tab Configuration nodes. `ui-base` đang dùng phải có `ui-template` trong *Accept Client Data*, để câu trả lời chat chỉ gửi về đúng người hỏi.

## Việc cần tự làm một lần

Các bước này cần tài khoản của bạn nên phải tự làm:

- **HiveMQ Cloud:** tạo cluster free, tạo hai user MQTT (một cho ESP32, một cho Node-RED). Mở node cấu hình **Broker hộp cho ăn**, đặt **Server** `xxxxxxxx.s1.eu.hivemq.cloud`, **Port** `8883`, bật **Use TLS**, điền user và pass ở tab **Security**. Mặc định flow dùng `broker.hivemq.com` (công khai, ai cũng gửi lệnh được), chỉ dùng để thử.
- **ntfy:** cài app ntfy trên điện thoại, subscribe đúng tên `NTFY_TOPIC`.
- **Gemini:** lấy key tại <https://aistudio.google.com>. Nếu chat báo "đang quá tải" và log có mã 429 thì đổi `GEMINI_MODEL` sang model có gói miễn phí.
- **Supabase:** cùng Thuận tạo project và chạy [`supabase/schema.sql`](../supabase/schema.sql).
- **Gmail (ID 6):** bật xác minh 2 bước cho tài khoản Gmail gửi, tạo **App Password** (myaccount.google.com → Security → App passwords). Mở node **Gmail SMTP** trong tab ID 6, điền **Userid** là địa chỉ Gmail và **Password** là App Password; hai mục này nằm trong `flows_cred.json`, không lên git. Đặt `EMAIL_TO` trong `.env` là địa chỉ nhận báo cáo.
- **Giờ máy:** cron 20:00 chạy theo giờ của máy chạy Node-RED, nên máy phải để múi giờ Việt Nam.

## Dữ liệu ghi lên Supabase (ID 3)

ID 4 và ID 8 đọc bảng `feeder_events` trong `supabase/schema.sql`. ID 3 ghi:

| Sự kiện | Khi nào ghi | Dữ liệu |
|---|---|---|
| Đo mức | Tin `feeder/level` đầu tiên, sau đó tối đa 2 phút một dòng | `{"event":"level","level":72}` |
| Cho ăn | Tin `feeder/status` có `result === "done"` | `{"event":"feed","duration_ms":1500}` |

Nếu đổi tên bảng hoặc cột thì sửa ba function: "Tạo truy vấn 24 giờ" (ID 4), "Nhận câu hỏi" và "Lưu mức, hỏi lịch sử" (ID 8). Cũng phải sửa các function Supabase trong tab ID 3 và ID 6.

Bố cục trang Dashboard theo wireframe mục 2.2 của proposal. Cột trái gồm Mức thức ăn, Cho ăn, Lịch sử. Cột phải gồm Biểu đồ, Thông báo (ntfy và email), Chat. Thanh "Tài khoản" nằm trên cùng.

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

**ID 1:** số đo không phải số hoặc nằm ngoài 0–100 thì bị bỏ qua và có cảnh báo trong log. Mức mới nhất được lưu vào global `muc_moi_nhat`.

**ID 3:** ESP32 gửi 5 giây một lần, nhưng chỉ ghi 2 phút một dòng, tức 720 dòng mỗi ngày. Lý do: ID 4 đọc tối đa 1000 dòng cho 24 giờ. Ghi lỗi thì số đo kế tiếp được ghi lại ngay, không phải đợi 2 phút. Lần cho ăn luôn được ghi. Bảng lịch sử làm mới mỗi 60 giây và sau mỗi lần ghi; mất mạng thì vẫn giữ bảng cũ.

**ID 6:** 20:00 hằng ngày, nếu công tắc bật thì đọc Supabase rồi gửi email. Email gồm mức hiện tại, số lần và giờ cho ăn trong ngày tính từ 0:00 giờ Việt Nam, và thêm dòng cảnh báo nếu mức dưới 20%. Nút "Gửi thử email" gửi ngay, kể cả khi công tắc tắt. Công tắc quay về bật khi khởi động lại Node-RED.

**ID 7:**

- Mật khẩu được băm bằng scrypt với muối ngẫu nhiên 16 byte. Truy vấn SQLite có tham số, nên không bị SQL injection.
- Tên đăng nhập 3–32 ký tự, chỉ gồm chữ, số và `_`. Mật khẩu ít nhất 6 ký tự.
- File `users.db` nằm ở thư mục chạy Node-RED (gốc repo nếu chạy `start.ps1` như trên), đã có trong `.gitignore`.
- Phiên đăng nhập gắn với kết nối Dashboard của từng trình duyệt. F5 hoặc khởi động lại Node-RED thì phải đăng nhập lại.
- Đây là kiểm soát ở mức giao diện cho đồ án, không thay cho xác thực HTTP thật (`httpNodeAuth`).

## Bảo mật

Repo đang public. Key chỉ nằm trong `.env`, user và pass MQTT nằm trong `flows_cred.json`; cả hai đều không lên git. Trước khi commit flow export lại, kiểm tra không có key trong file.

## Trạng thái kiểm thử

Đã chạy Node-RED thật, bấm trên Dashboard thật, với Supabase, ntfy, Gemini **giả lập cục bộ**. ntfy.sh thật đã nhận đúng định dạng tin nhắn.

**Chưa thử với dịch vụ thật:** Supabase, Gemini (kể cả tên model và gói miễn phí), HiveMQ Cloud, và luồng ID 4, 5, 8 với dữ liệu thật từ ESP32.

**Phần của Thuận (06/11/2026):**

- Đã chạy `flows_quan.json` và `flows_thuan.json` chung trên Node-RED thật, với broker MQTT, Supabase và SMTP giả lập cục bộ. Đã kiểm tra trên trình duyệt: chuyển sang trang đăng nhập, đăng ký, đăng nhập, gauge, bảng lịch sử và nút gửi thử email.
- Gom mức 2 phút/lần, ghi lại khi lỗi, và chặn đúng các số đo sai.
- Chưa thử Gmail thật, Supabase thật, cron 20:00 theo giờ thật, và firmware ID 1 trên board.
