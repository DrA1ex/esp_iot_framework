#pragma once

#include "macro.h"

#define __ENUM_VALUE(Name, _1, _2) _1 = _2,
#define __ENUM_CASE(Name, _1, _2) case Name::_1: return #_1;

#define __ENUM_VALUE_AUTO(Name, _1) _1,
#define __ENUM_CASE_AUTO(Name, _1) case Name::_1: return #_1;


#ifdef DEBUG

#include <cstring>

#define MAKE_ENUM(Name, Type, ...)                                                \
    enum class Name: Type { FOR_EACH_OPTS_2(__ENUM_VALUE, Name, __VA_ARGS__)  };  \
    inline const char * __debug_enum_str(Name _e) {                               \
        switch (_e) {                                                             \
            FOR_EACH_OPTS_2(__ENUM_CASE, Name, __VA_ARGS__)                       \
            default: {                                                            \
                static char _str[32] = {0};                                       \
                sniprintf(_str, sizeof(_str), "Unknown (%i)", (int) _e);          \
                return _str;                                                      \
            }                                                                     \
        }                                                                         \
    }

#define MAKE_ENUM_AUTO(Name, Type, ...)                                                \
    enum class Name: Type { FOR_EACH_OPTS_1(__ENUM_VALUE_AUTO, Name, __VA_ARGS__)  };  \
    inline const char * __debug_enum_str(Name _e) {                               \
        switch (_e) {                                                             \
            FOR_EACH_OPTS_1(__ENUM_CASE_AUTO, Name, __VA_ARGS__)                  \
            default: {                                                            \
                static char _str[32] = {0};                                       \
                sniprintf(_str, sizeof(_str), "Unknown (%i)", (int) _e);          \
                return _str;                                                      \
            }                                                                     \
        }                                                                         \
    }
#else
#define MAKE_ENUM(Name, Type, ...)                                                  \
    enum class Name: Type { FOR_EACH_OPTS_2(__ENUM_VALUE, Name, __VA_ARGS__)  };    \
    constexpr const char * __debug_enum_str(Name _e) {                              \
        return "*** Debug info deleted ***";                                        \
    }

#define MAKE_ENUM_AUTO(Name, Type, ...)                                               \
    enum class Name: Type { FOR_EACH_OPTS_1(__ENUM_VALUE_AUTO, Name, __VA_ARGS__)  }; \
    constexpr const char * __debug_enum_str(Name _e) {                                \
        return "*** Debug info deleted ***";                                          \
    }
#endif