#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#ifndef NTP_UPDATE_INTERVAL
#define NTP_UPDATE_INTERVAL                     (24ul * 3600 * 1000)
#endif

class NtpTime {
    WiFiUDP _wifi_udp;
    NTPClient _ntp_client;

    int32_t _tz_offset = 0;

public:
    NtpTime();

    void begin(float tz);
    void update();

    [[nodiscard]] inline bool available() const { return _ntp_client.isTimeSet(); }

    [[nodiscard]] inline int seconds() const { return _ntp_client.getSeconds(); }
    [[nodiscard]] inline int minutes() const { return _ntp_client.getMinutes(); }
    [[nodiscard]] inline int hours() const { return _ntp_client.getHours(); }
    [[nodiscard]] inline int week_day() const { return _ntp_client.getDay(); }

    [[nodiscard]] inline int32_t tz_offset() const { return _tz_offset; }

    [[nodiscard]] inline uint32_t seconds_from_midnight_tz() const { return seconds() + minutes() * 60 + hours() * 3600; }
    [[nodiscard]] inline unsigned long today_tz() const { return epoch_tz() - seconds_from_midnight_tz(); }
    [[nodiscard]] inline unsigned long epoch_tz() const { return _ntp_client.getEpochTime(); }

    [[nodiscard]] tm *date() const;

    static const uint32_t SECONDS_PER_DAY;
};