/*
 * Smart Pet Feeder — ID 2: Cho ăn từ xa bằng servo SG90 (Quân, 24127512)
 *
 * File do AI (Claude) sinh ngày 04/11/2026. Chưa có đoạn sinh viên tự viết,
 * chưa chép đoạn nào từ nguồn ngoài. Sinh viên đọc hiểu, chỉnh sửa và ghi rõ
 * phần tự sửa trước khi nộp.
 *
 * Luồng 2 (proposal mục 1.2):
 *   Dashboard bấm "CHO ĂN" -> Node-RED publish {"duration_ms":1500}
 *   lên topic feeder/cmd/feed -> ESP32 mở cửa gạt đúng duration_ms rồi đóng
 *   -> publish xác nhận lên topic feeder/status.
 *
 * Board: ESP32 NodeMCU LuaNode32 (chọn "ESP32 Dev Module" trong Arduino IDE).
 * Thư viện cần cài (Library Manager): PubSubClient (Nick O'Leary),
 *                                      ESP32Servo (Kevin Harrington).
 * Điền WiFi + MQTT vào secrets.h (copy từ secrets.example.h).
 *
 * Code chia thành id2Setup() / id2Loop() / id2XuLyLenh() để tuần 8 gộp với
 * firmware ID 1 (HC-SR04) của Thuận: chỉ cần gọi hai hàm này trong setup/loop
 * chung và cho callback MQTT chuyển topic feeder/cmd/feed vào id2XuLyLenh().
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "secrets.h"

// ---------------- Cấu hình phần cứng ----------------
const int SERVO_PIN = 13;     // dây cam (tín hiệu) của SG90
const int GOC_DONG  = 0;      // góc cửa gạt đóng — chỉnh khi lắp cơ cấu thật
const int GOC_MO    = 90;     // góc cửa gạt mở

// Giới hạn thời gian mở: lệnh ngoài khoảng này bị kẹp lại, tránh xả hết phễu
const unsigned long MO_TOI_THIEU_MS = 200;
const unsigned long MO_TOI_DA_MS    = 5000;
// Thời gian chờ servo quay về góc đóng trước khi báo "done"
const unsigned long CHO_DONG_MS     = 400;

// ---------------- Topic MQTT (đã thống nhất với Thuận) ----------------
const char* TOPIC_CMD    = "feeder/cmd/feed";
const char* TOPIC_STATUS = "feeder/status";
const char* TOPIC_ONLINE = "feeder/online";   // retained, có LWT báo mất kết nối

#if MQTT_USE_TLS
WiFiClientSecure net;   // HiveMQ Cloud bắt buộc TLS, cổng 8883
#else
WiFiClient net;         // broker công khai / Wokwi, cổng 1883
#endif
PubSubClient mqtt(net);
Servo cuaGat;

// ---------------- Trạng thái cửa gạt ----------------
// Không dùng delay(): trong lúc cửa mở, loop() vẫn chạy để giữ kết nối MQTT
// và sau này còn đọc HC-SR04 của ID 1.
enum TrangThaiCua { CUA_DONG, CUA_DANG_MO, CUA_DANG_VE };
TrangThaiCua trangThai = CUA_DONG;
unsigned long mocChuyenTrangThai = 0;   // millis() lúc vào trạng thái hiện tại
unsigned long thoiGianMo = 0;           // duration_ms của lệnh đang chạy

unsigned long lanThuMqttCuoi = 0;
char clientId[32];

// ---------------- Gửi xác nhận ----------------
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

// ---------------- Đọc duration_ms từ payload ----------------
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

// ---------------- Xử lý lệnh cho ăn ----------------
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
  // Tuần 8: thêm các topic khác của ID 1 ở đây nếu có.
}

// ---------------- Kết nối ----------------
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

// ---------------- Vòng đời ID 2 ----------------
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

void setup() {
  Serial.begin(115200);
  id2Setup();

  uint64_t mac = ESP.getEfuseMac();
  snprintf(clientId, sizeof(clientId), "feeder-%04X%08X",
           (unsigned int)(mac >> 32), (unsigned int)mac);

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
  id2Loop();
}
