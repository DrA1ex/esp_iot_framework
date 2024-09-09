#include "ntp_time.h"

#include "../debug.h"

const uint32_t NtpTime::SECONDS_PER_DAY = (uint32_t) 24 * 60 * 60;

NtpTime::NtpTime() : _ntp_client(_wifi_udp) {}

void NtpTime::begin(float tz) {
    _ntp_client.begin();

    _tz_offset = (int32_t) tz * 3600;
    _ntp_client.setTimeOffset(_tz_offset);

    //TODO: Switch to async lib
    _ntp_client.setUpdateInterval(NTP_UPDATE_INTERVAL);
}

void NtpTime::update() {
    bool success = _ntp_client.update();

    if (success) D_PRINTF("NtpTime loaded. Now: %s\r\n", D_TIME_STRING(epoch_tz()));
}

tm *NtpTime::date() const {
    const time_t time = epoch_tz();
    return gmtime(&time);
}
