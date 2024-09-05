#pragma once

#define __DEBUG_LEVEL_VERBOSE 0

#ifdef DEBUG

#include <Arduino.h>
#include <ctime>

#define D_PRINT(x) Serial.println(x)
#define D_PRINTF(...) Serial.printf(__VA_ARGS__)
#define D_WRITE(x) Serial.print(x)

#define D_PRINT_HEX(ptr, length)                        \
        D_WRITE("HEX: ");                               \
        for (unsigned int i = 0; i < length; ++i) {     \
            D_PRINTF("%02X ", (ptr)[i]);                \
        }                                               \
        D_PRINT()

#define D_TIME_STRING(unix_time) ([](time_t time) {                             \
        static char buffer[20];                                                 \
        struct tm* time_info;                                                   \
        time_info = localtime(&time);                                           \
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info);       \
        return buffer;                                                          \
}(unix_time))

#if DEBUG_LEVEL == __DEBUG_LEVEL_VERBOSE
#define VERBOSE(ARG) ARG
#else
#define VERBOSE(ARG)
#endif
#else
#define D_PRINT(x)
#define D_PRINTF(...)
#define D_WRITE(x)
#define D_PRINT_HEX(ptr, length)

#define VERBOSE(ARG)
#endif

template<typename T>
const char *__debug_enum_str(T t) { return "Unsupported"; }