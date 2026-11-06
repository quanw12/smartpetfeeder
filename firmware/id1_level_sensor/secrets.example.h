// Mẫu cấu hình. Copy thành secrets.h rồi điền thông tin thật.
// KHÔNG đưa secrets.h thật lên GitHub / nộp bài kèm mật khẩu.
#pragma once

#define WIFI_SSID "ten-wifi"
#define WIFI_PASS "mat-khau-wifi"

// HiveMQ Cloud (free): Cluster URL dạng xxxxxxxx.s1.eu.hivemq.cloud
// Tạo user/pass ở tab "Access Management" của cluster.
#define MQTT_HOST "xxxxxxxx.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "feeder-esp32"
#define MQTT_PASS "mat-khau-mqtt"
#define MQTT_USE_TLS 1

// Test trên Wokwi / broker công khai thì dùng bộ này:
// #define WIFI_SSID "Wokwi-GUEST"
// #define WIFI_PASS ""
// #define MQTT_HOST "broker.hivemq.com"
// #define MQTT_PORT 1883
// #define MQTT_USER nullptr
// #define MQTT_PASS nullptr
// #define MQTT_USE_TLS 0
