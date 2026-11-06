/*
 * Smart Pet Feeder — Firmware gộp ID 1 + ID 2 (nạp file này cho sản phẩm thật)
 *   ID 1: Đo mức thức ăn bằng HC-SR04 (Thuận, 24127551)
 *   ID 2: Cho ăn từ xa bằng servo SG90 (Quân, 24127512)
 *
 * File do AI (Claude) gộp ngày 06/11/2026 từ firmware/id1_level_sensor và
 * firmware/id2_servo_feed. Chưa có đoạn sinh viên tự viết, chưa chép đoạn nào
 * từ nguồn ngoài. Sinh viên đọc hiểu, chỉnh sửa và ghi rõ phần tự sửa trước
 * khi nộp.
 *
 * Luồng 1: HC-SR04 -> ESP32 đổi ra % -> publish {"level":72,"distance_cm":8.5}
 *          lên feeder/level mỗi 5 s.
 * Luồng 2: Dashboard -> {"duration_ms":1500} trên feeder/cmd/feed -> ESP32 mở
 *          cửa gạt rồi đóng -> xác nhận trên feeder/status.
 *
 * Board: ESP32 NodeMCU LuaNode32 (chọn "ESP32 Dev Module" trong Arduino IDE).
 * Thư viện (Library Manager): PubSubClient (Nick O'Leary),
 *                             ESP32Servo (Kevin Harrington).
 * Điền WiFi + MQTT vào secrets.h (copy từ secrets.example.h).
 * Nối dây: xem README.md cùng thư mục.
 *
 * Không dùng delay() trong loop(): trong lúc cửa mở, loop() vẫn đo mức và giữ
 * kết nối MQTT. Mỗi lần đo chặn tối đa 25 ms nên cửa đóng trễ không đáng kể.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "secrets.h"

// ---------------- Cấu hình phần cứng ----------------
// ID 1: HC-SR04
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
// Khoảng cách cảm biến -> mặt thức ăn (cm). Chỉnh theo phễu thật.
const float KHOANG_CACH_RONG_CM = 20.0;   // phễu rỗng: sóng chạm đáy
const float KHOANG_CACH_DAY_CM  = 4.0;    // phễu đầy: cách cảm biến 4 cm

// ID 2: SG90
const int SERVO_PIN = 13;     // dây cam (tín hiệu) của SG90
const int GOC_DONG  = 0;      // góc cửa gạt đóng — chỉnh khi lắp cơ cấu thật
const int GOC_MO    = 90;     // góc cửa gạt mở

// ---------------- Topic MQTT ----------------
const char* TOPIC_LEVEL  = "feeder/level";
const char* TOPIC_CMD    = "feeder/cmd/feed";
const char* TOPIC_STATUS = "feeder/status";
const char* TOPIC_ONLINE = "feeder/online";   // retained, có LWT báo mất kết nối

// ---------------- Kết nối dùng chung ----------------
#if MQTT_USE_TLS
WiFiClientSecure net;   // HiveMQ Cloud bắt buộc TLS, cổng 8883
#else
WiFiClient net;         // broker công khai / Wokwi, cổng 1883
#endif
PubSubClient mqtt(net);

unsigned long lanThuMqttCuoi = 0;
char clientId[32];

// =====================================================================
// ID 1 — Đo mức thức ăn (Thuận)
// =====================================================================

// HC-SR04 đo tin cậy từ 2 cm đến 400 cm, ngoài khoảng này coi là nhiễu
const float DO_GAN_NHAT_CM = 2.0;
const float DO_XA_NHAT_CM  = 400.0;
// 25 ms đủ cho sóng đi về ~4 m; hết giờ thì pulseIn trả 0
const unsigned long HET_GIO_ECHO_US = 25000;

const unsigned long CHU_KY_DO_MS  = 200;    // mỗi 200 ms đo một lần
const unsigned long CHU_KY_GUI_MS = 5000;   // mỗi 5 s gửi một lần
const int SO_MAU = 5;                       // xét 5 lần đo gần nhất
const int SO_MAU_TOI_THIEU = 3;             // ít hơn thì chưa đủ tin để gửi

// Vòng tròn: lần đo mới ghi đè lần cũ nhất. Lần đo lỗi được ghi là -1, nên khi
// tuột dây cảm biến các số cũ bị đẩy ra sau khoảng 1 s và hộp ngừng gửi mức.
float mau_do[SO_MAU];
int vi_tri_ghi = 0;
int so_lan_do = 0;

unsigned long lan_do_cuoi = 0;
unsigned long lan_gui_cuoi = 0;

// Một lần đo mất tối đa ~25 ms (hết giờ echo), đủ ngắn để không làm trễ loop
float do_mot_lan() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);          // xung kích 10 µs theo datasheet
  digitalWrite(TRIG_PIN, LOW);

  unsigned long thoi_gian_us = pulseIn(ECHO_PIN, HIGH, HET_GIO_ECHO_US);
  if (thoi_gian_us == 0) return -1;         // không có tiếng vọng
  // Âm thanh 343 m/s = 0,0343 cm/µs, chia 2 vì sóng đi và về
  float khoang_cach = thoi_gian_us * 0.0343f / 2.0f;
  if (khoang_cach < DO_GAN_NHAT_CM || khoang_cach > DO_XA_NHAT_CM) return -1;
  return khoang_cach;
}

// Ghi cả lần đo lỗi (-1) để biết trong 5 lần gần nhất có bao nhiêu lần hợp lệ
void them_mau(float khoang_cach) {
  mau_do[vi_tri_ghi] = khoang_cach;
  vi_tri_ghi = (vi_tri_ghi + 1) % SO_MAU;
  if (so_lan_do < SO_MAU) so_lan_do++;
}

// Trung vị của các lần đo hợp lệ; một lần đo sai (sóng dội vào thành phễu)
// không kéo lệch kết quả như trung bình. Trả -1 nếu không đủ lần hợp lệ.
float tinh_trung_vi() {
  float tam[SO_MAU];
  int n = 0;
  for (int i = 0; i < so_lan_do; i++) {
    if (mau_do[i] > 0) tam[n++] = mau_do[i];
  }
  if (n < SO_MAU_TOI_THIEU) return -1;
  // Sắp xếp chèn, n <= 5 nên đủ nhanh
  for (int i = 1; i < n; i++) {
    float x = tam[i];
    int j = i - 1;
    while (j >= 0 && tam[j] > x) {
      tam[j + 1] = tam[j];
      j--;
    }
    tam[j + 1] = x;
  }
  if (n % 2 == 1) return tam[n / 2];
  return (tam[n / 2 - 1] + tam[n / 2]) / 2.0f;
}

// Khoảng cách càng nhỏ thì thức ăn càng đầy
int tinh_muc(float khoang_cach) {
  float muc = 100.0f * (KHOANG_CACH_RONG_CM - khoang_cach)
              / (KHOANG_CACH_RONG_CM - KHOANG_CACH_DAY_CM);
  int muc_nguyen = (int)lroundf(muc);
  if (muc_nguyen < 0) muc_nguyen = 0;
  if (muc_nguyen > 100) muc_nguyen = 100;
  return muc_nguyen;
}

void gui_muc() {
  float khoang_cach = tinh_trung_vi();
  if (khoang_cach < 0) {
    Serial.println("[ID1] Khong du lan do hop le, kiem tra day Trig/Echo");
    return;
  }
  int muc = tinh_muc(khoang_cach);

  char buf[64];
  snprintf(buf, sizeof(buf), "{\"level\":%d,\"distance_cm\":%.1f}", muc, khoang_cach);
  Serial.print("[ID1] -> ");
  Serial.println(buf);
  if (mqtt.connected()) mqtt.publish(TOPIC_LEVEL, buf);
  else Serial.println("[ID1] Chua ket noi MQTT, bo qua lan gui nay");
}

void id1_setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void id1_loop() {
  unsigned long bay_gio = millis();
  // Phép trừ unsigned vẫn đúng khi millis() tràn sau ~49 ngày
  if (bay_gio - lan_do_cuoi >= CHU_KY_DO_MS) {
    lan_do_cuoi = bay_gio;
    them_mau(do_mot_lan());
  }
  if (bay_gio - lan_gui_cuoi >= CHU_KY_GUI_MS) {
    lan_gui_cuoi = bay_gio;
    gui_muc();
  }
}

// =====================================================================
// ID 2 — Cho ăn từ xa (Quân)
// =====================================================================

// Giới hạn thời gian mở: lệnh ngoài khoảng này bị kẹp lại, tránh xả hết phễu
const unsigned long MO_TOI_THIEU_MS = 200;
const unsigned long MO_TOI_DA_MS    = 5000;
// Thời gian chờ servo quay về góc đóng trước khi báo "done"
const unsigned long CHO_DONG_MS     = 400;

Servo cuaGat;

enum TrangThaiCua { CUA_DONG, CUA_DANG_MO, CUA_DANG_VE };
TrangThaiCua trangThai = CUA_DONG;
unsigned long mocChuyenTrangThai = 0;   // millis() lúc vào trạng thái hiện tại
unsigned long thoiGianMo = 0;           // duration_ms của lệnh đang chạy

void guiTrangThai(const char* ketQua, unsigned long duration, const char* lyDo) {
  char buf[128];
  if (lyDo) {
    snprintf(buf, sizeof(buf),
             "{\"event\":\"feed\",\"result\":\"%s\",\"reason\":\"%s\"}", ketQua, lyDo);
  } else {
    snprintf(buf, sizeof(buf),
             "{\"event\":\"feed\",\"result\":\"%s\",\"duration_ms\":%lu}", ketQua, duration);
  }
  Serial.print("[ID2] -> ");
  Serial.println(buf);
  if (mqtt.connected()) mqtt.publish(TOPIC_STATUS, buf);
}

// Nhận {"duration_ms":1500} hoặc chỉ số 1500. Trả về -1 nếu không đọc được.
// Payload rất ngắn nên tự tách, không cần thêm thư viện JSON.
long docDuration(const char* s) {
  const char* p = strstr(s, "duration_ms");
  if (p) {
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
  } else {
    p = s;
  }
  while (*p == ' ' || *p == '"') p++;
  if (*p < '0' || *p > '9') return -1;
  long v = 0;
  while (*p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    if (v > 600000) return -1;   // số vô lý (> 10 phút)
    p++;
  }
  return v;
}

void id2XuLyLenh(const char* payload) {
  long d = docDuration(payload);
  if (d < 0) {
    guiTrangThai("rejected", 0, "invalid");
    return;
  }
  if (trangThai != CUA_DONG) {          // đang mở thì bỏ lệnh mới, không cộng dồn
    guiTrangThai("rejected", 0, "busy");
    return;
  }
  unsigned long dur = (unsigned long)d;
  if (dur < MO_TOI_THIEU_MS) dur = MO_TOI_THIEU_MS;
  if (dur > MO_TOI_DA_MS)    dur = MO_TOI_DA_MS;

  thoiGianMo = dur;
  cuaGat.write(GOC_MO);
  trangThai = CUA_DANG_MO;
  mocChuyenTrangThai = millis();
  guiTrangThai("opening", dur, nullptr);
}

void id2Setup() {
  cuaGat.setPeriodHertz(50);              // SG90: xung 50 Hz
  cuaGat.attach(SERVO_PIN, 500, 2400);    // độ rộng xung 0.5–2.4 ms ứng với 0–180°
  cuaGat.write(GOC_DONG);                 // khởi động luôn đóng cửa
  trangThai = CUA_DONG;
}

void id2Loop() {
  unsigned long now = millis();
  // Phép trừ unsigned vẫn đúng khi millis() tràn sau ~49 ngày.
  if (trangThai == CUA_DANG_MO && now - mocChuyenTrangThai >= thoiGianMo) {
    cuaGat.write(GOC_DONG);
    trangThai = CUA_DANG_VE;
    mocChuyenTrangThai = now;
  } else if (trangThai == CUA_DANG_VE && now - mocChuyenTrangThai >= CHO_DONG_MS) {
    trangThai = CUA_DONG;
    guiTrangThai("done", thoiGianMo, nullptr);
  }
}

// Test không cần MQTT: mở Serial Monitor 115200, gõ "1500" rồi Enter
void docLenhSerial() {
  static char buf[32];
  static unsigned int n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (n > 0) {
        buf[n] = '\0';
        id2XuLyLenh(buf);
        n = 0;
      }
    } else if (n < sizeof(buf) - 1) {
      buf[n++] = c;
    }
  }
}

// =====================================================================
// WiFi + MQTT dùng chung
// =====================================================================

void khiNhanMqtt(char* topic, byte* payload, unsigned int len) {
  char buf[128];
  unsigned int n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  Serial.print("[MQTT] <- ");
  Serial.print(topic);
  Serial.print(" ");
  Serial.println(buf);

  if (strcmp(topic, TOPIC_CMD) == 0) id2XuLyLenh(buf);
}

void ketNoiWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Dang ket noi");
  unsigned long bd = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - bd < 15000) {
    delay(300);                     // chỉ chờ trong setup(), không nằm trong loop()
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " that bai, se thu lai sau");
}

void giuKetNoiMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  // mqtt.connect() có thể chặn vài giây (bắt tay TLS): chỉ thử khi cửa đang
  // đóng để không làm trễ việc đóng cửa.
  if (trangThai != CUA_DONG) return;
  if (lanThuMqttCuoi != 0 && millis() - lanThuMqttCuoi < 5000) return;
  lanThuMqttCuoi = millis();

  Serial.print("[MQTT] Ket noi ");
  Serial.print(MQTT_HOST);
  // LWT: nếu ESP32 mất điện/mất mạng, broker tự publish {"online":false}
  if (mqtt.connect(clientId, MQTT_USER, MQTT_PASS,
                   TOPIC_ONLINE, 0, true, "{\"online\":false}")) {
    Serial.println(" OK");
    mqtt.publish(TOPIC_ONLINE, "{\"online\":true}", true);
    mqtt.subscribe(TOPIC_CMD);
  } else {
    Serial.print(" loi rc=");
    Serial.println(mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  id1_setup();
  id2Setup();                // đóng cửa ngay, trước khi chờ WiFi

  uint64_t mac = ESP.getEfuseMac();
  snprintf(clientId, sizeof(clientId), "feeder-%04X%08X",
           (uint16_t)(mac >> 32), (uint32_t)mac);

  ketNoiWifi();
#if MQTT_USE_TLS
  net.setInsecure();   // bỏ kiểm tra chứng chỉ cho đơn giản; vẫn mã hoá TLS
#endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(khiNhanMqtt);
  mqtt.setKeepAlive(30);
}

void loop() {
  giuKetNoiMqtt();
  mqtt.loop();
  docLenhSerial();
  id2Loop();                 // kiểm tra đóng cửa trước khi đo
  id1_loop();
}
