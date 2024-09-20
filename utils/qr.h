#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <qrcode.h>

inline void qr_print_string(const char *str) {
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();

    auto ret = esp_qrcode_generate(&cfg, str);
    if (ret != 0) {
        D_PRINTF("Unable to generate QR-code, return code: %i\r\n", ret);
        return;
    }
}

inline void qr_print_wifi_connection(const char *ssid, const char *password) {
    String connection_str = "WIFI:T:WPA;S:";
    connection_str += ssid;
    connection_str += ";P:";
    connection_str += password;
    connection_str += ";;";

    Serial.flush();
    qr_print_string(connection_str.c_str());
}

#else
inline void qr_print_string(const char *) {
    D_PRINT("QR code not supported");
}

inline void qr_print_wifi_connection(const char *, const char *) {
    qr_print_string(nullptr);
}
#endif