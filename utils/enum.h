#define __PARENS ()

#define __EXPAND(...) __EXPAND4(__EXPAND4(__EXPAND4(__EXPAND4(__VA_ARGS__))))
#define __EXPAND4(...) __EXPAND3(__EXPAND3(__EXPAND3(__EXPAND3(__VA_ARGS__))))
#define __EXPAND3(...) __EXPAND2(__EXPAND2(__EXPAND2(__EXPAND2(__VA_ARGS__))))
#define __EXPAND2(...) __EXPAND1(__EXPAND1(__EXPAND1(__EXPAND1(__VA_ARGS__))))
#define __EXPAND1(...) __VA_ARGS__

#define __MAKE_ENUM_OPTS(MACRO, Name, ...) __EXPAND(__MAKE_ENUM_IMPL(MACRO, Name, __VA_ARGS__))
#define __MAKE_ENUM_IMPL(MACRO, Name, _1, _2, ...) MACRO(Name, _1, _2) \
    __VA_OPT__(__MAKE_ENUM_IMPL_AGAIN __PARENS (MACRO, Name, __VA_ARGS__))
#define __MAKE_ENUM_IMPL_AGAIN() __MAKE_ENUM_IMPL

#define __ENUM_VALUE(Name, _1, _2) _1 = _2,
#define __ENUM_CASE(Name, _1, _2) case Name::_1: return #_1;


#ifdef DEBUG
#define MAKE_ENUM(Name, Type, ...)                                                  \
    enum class Name: Type { __MAKE_ENUM_OPTS(__ENUM_VALUE, Name, __VA_ARGS__)  };   \
    constexpr const char * __debug_enum_str(Name _e) {                              \
        switch (_e) {                                                               \
            __MAKE_ENUM_OPTS(__ENUM_CASE, Name, __VA_ARGS__)                        \
        default:                                                                    \
            return "unknown";                                                       \
        }                                                                           \
    }
#else
#define MAKE_ENUM(Name, Type, ...)                                                  \
    enum class Name: Type { __MAKE_ENUM_OPTS(__ENUM_VALUE, Name, __VA_ARGS__)  };   \
    constexpr const char * __debug_enum_str(Name _e) {                              \
        return "*** Debug info deleted ***";                                        \
    }
#endif