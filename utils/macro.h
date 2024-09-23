#pragma once

#define __PARENS ()

#define __EXPAND(...) __EXPAND4(__EXPAND4(__EXPAND4(__EXPAND4(__VA_ARGS__))))
#define __EXPAND4(...) __EXPAND3(__EXPAND3(__EXPAND3(__EXPAND3(__VA_ARGS__))))
#define __EXPAND3(...) __EXPAND2(__EXPAND2(__EXPAND2(__EXPAND2(__VA_ARGS__))))
#define __EXPAND2(...) __EXPAND1(__EXPAND1(__EXPAND1(__EXPAND1(__VA_ARGS__))))
#define __EXPAND1(...) __VA_ARGS__


#define FOR_EACH_OPTS_1(MACRO, Arg, ...) __EXPAND(FOR_EACH_IMPL_1(MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_1(MACRO, Arg, _1, ...) MACRO(Arg, _1) \
    __VA_OPT__(FOR_EACH_IMPL_AGAIN_1 __PARENS (MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_AGAIN_1() FOR_EACH_IMPL_1

#define FOR_EACH_OPTS_2(MACRO, Arg, ...) __EXPAND(FOR_EACH_IMPL_2(MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_2(MACRO, Arg, _1, _2, ...) MACRO(Arg, _1, _2) \
    __VA_OPT__(FOR_EACH_IMPL_AGAIN_2 __PARENS (MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_AGAIN_2() FOR_EACH_IMPL_2

#define FOR_EACH_OPTS_3(MACRO, Arg, ...) __EXPAND(FOR_EACH_IMPL_3(MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_3(MACRO, Arg, _1, _2, _3, ...) MACRO(Arg, _1, _2, _3) \
    __VA_OPT__(FOR_EACH_IMPL_AGAIN_3 __PARENS (MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_AGAIN_3() FOR_EACH_IMPL_3

#define FOR_EACH_OPTS_4(MACRO, Arg, ...) __EXPAND(FOR_EACH_IMPL_4(MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_4(MACRO, Arg, _1, _2, _3, _4, ...) MACRO(Arg, _1, _2, _3, _4) \
    __VA_OPT__(FOR_EACH_IMPL_AGAIN_4 __PARENS (MACRO, Arg, __VA_ARGS__))
#define FOR_EACH_IMPL_AGAIN_4() FOR_EACH_IMPL_4