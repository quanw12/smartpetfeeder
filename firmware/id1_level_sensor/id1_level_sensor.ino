/*
 * Smart Pet Feeder — ID 1: Đo mức thức ăn bằng HC-SR04 (Thuận, 24127551)
 *
 * File do AI (Claude) sinh ngày 06/11/2026. Chưa có đoạn sinh viên tự viết,
 * chưa chép đoạn nào từ nguồn ngoài. Sinh viên đọc hiểu, chỉnh sửa và ghi rõ
 * phần tự sửa trước khi nộp.
 *
 * Luồng 1 (proposal mục 1.2):
 *   HC-SR04 gắn trên nắp phễu, đo khoảng cách xuống mặt thức ăn
 *   -> ESP32 đổi ra % mức -> publish {"level":72,"distance_cm":8.5}
 *   lên topic feeder/level mỗi 5 s -> Node-RED hiện gauge (ID 1),
 *   lưu Supabase (ID 3), cảnh báo ntfy (ID 5 của Quân).
 *
 * Nối dây:
 *   HC-SR04 VCC  -> VIN/5V của ESP32
 *   HC-SR04 GND  -> GND (chung với servo SG90)
 *   HC-SR04 Trig -> GPIO 5
 *   HC-SR04 Echo -> cầu chia áp 1 kΩ / 2,2 kΩ -> GPIO 18
 *     (Echo ra 5 V, chân ESP32 chỉ chịu 3,3 V: 5 V x 2,2/3,2 ≈ 3,4 V)
 *
 * Hiệu chỉnh: đo khoảng cách khi phễu rỗng và khi đầy, sửa hai hằng
 * KHOANG_CACH_RONG_CM / KHOANG_CACH_DAY_CM bên dưới.
 *
 * Code chia thành id1_setup() / id1_loop() để tuần 8 gộp với firmware ID 2
 * của Quân: gọi thêm hai hàm này trong setup/loop chung. id1_loop() không
 * dùng delay() dài nên không làm trễ việc đóng cửa của ID 2.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"

// ---------------- Cấu hình phần cứng ----------------
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// Khoảng cách cảm biến -> mặt thức ăn (cm). Chỉnh theo phễu thật.
const float KHOANG_CACH_RONG_CM = 20.0;   // phễu rỗng: sóng chạm đáy
const float KHOANG_CACH_DAY_CM  = 4.0;    // phễu đầy: cách cảm biến 4 cm

// HC-SR04 đo tin cậy từ 2 cm đến 400 cm, ngoài khoảng này coi là nhiễu
const float DO_GAN_NHAT_CM = 2.0;
const float DO_XA_NHAT_CM  = 400.0;
// 25 ms đủ cho sóng đi về ~4 m; hết giờ thì pulseIn trả 0
const unsigned long HET_GIO_ECHO_US = 25000;

const unsigned long CHU_KY_DO_MS  = 200;    // mỗi 200 ms đo một lần
const unsigned long CHU_KY_GUI_MS = 5000;   // mỗi 5 s gửi một lần
const int SO_MAU = 5;                       // lấy trung vị 5 lần đo gần nhất
const int SO_MAU_TOI_THIEU = 3;             // ít hơn thì chưa đủ tin để gửi

// ---------------- Topic MQTT (đã thống nhất với Quân) ----------------
const char* TOPIC_LEVEL = "feeder/level";

#if MQTT_USE_TLS
WiFiClientSecure net;   // HiveMQ Cloud bắt buộc TLS, cổng 8883
#else
WiFiClient net;         // broker công khai / Wokwi, cổng 1883
#endif
PubSubClient mqtt(net);

// ---------------- Bộ đệm mẫu đo ----------------
// Vòng tròn: lần đo mới ghi đè lần cũ nhất. Lần đo lỗi được ghi là -1, nên khi
// tuột dây cảm biến các số cũ bị đẩy ra sau khoảng 1 s và hộp ngừng gửi mức.
float mau_do[SO_MAU];
int vi_tri_ghi = 0;
int so_lan_do = 0;

unsigned long lan_do_cuoi = 0;
unsigned long lan_gui_cuoi = 0;
unsigned long lan_thu_mqtt_cuoi = 0;
char client_id[32];

// ---------------- Đo khoảng cách ----------------
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

// ---------------- Gửi mức ----------------
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

// ---------------- Kết nối ----------------
void ket_noi_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Dang ket noi");
  unsigned long bat_dau = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - bat_dau < 15000) {
    delay(300);                     // chỉ chờ trong setup(), không nằm trong loop()
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " that bai, se thu lai sau");
}

void giu_ket_noi_mqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  // Thử lại 5 s/lần để không chặn loop liên tục khi broker lỗi
  if (lan_thu_mqtt_cuoi != 0 && millis() - lan_thu_mqtt_cuoi < 5000) return;
  lan_thu_mqtt_cuoi = millis();

  Serial.print("[MQTT] Ket noi ");
  Serial.print(MQTT_HOST);
  // Không đặt LWT feeder/online ở đây: topic đó do firmware ID 2 quản lý
  if (mqtt.connect(client_id, MQTT_USER, MQTT_PASS)) {
    Serial.println(" OK");
  } else {
    Serial.print(" loi rc=");
    Serial.println(mqtt.state());
  }
}

// ---------------- Vòng đời ID 1 ----------------
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

void setup() {
  Serial.begin(115200);
  id1_setup();

  uint64_t mac = ESP.getEfuseMac();
  snprintf(client_id, sizeof(client_id), "level-%04X%08X",
           (unsigned int)(mac >> 32), (unsigned int)mac);

  ket_noi_wifi();
#if MQTT_USE_TLS
  net.setInsecure();   // bỏ kiểm tra chứng chỉ cho đơn giản; vẫn mã hoá TLS
#endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(30);
}

void loop() {
  giu_ket_noi_mqtt();
  mqtt.loop();
  id1_loop();
}
