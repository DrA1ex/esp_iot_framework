#pragma once

#include <functional>

#include "macro.h"
#include "../base/metadata.h"

typedef std::function<void(AbstractPropertyMeta *)> MetaVisitFn;

template<typename T>
inline auto _visit_impl(const MetaVisitFn &fn, T &t) -> decltype(std::declval<T>().visit(fn)) {
    return t.visit(fn);
}

template<typename T, std::size_t N>
inline void _visit_impl(const MetaVisitFn &fn, T (&array)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
        _visit_impl(fn, array[i]);
    }
}

inline void _visit_impl(const MetaVisitFn &fn, AbstractPropertyMeta &meta) {
    fn(&meta);
}

#define __MEMBER_DEFINE_IMPL_0(Type, _2, _3, _4) _2 _3;
#define __MEMBER_DEFINE_IMPL_1(Type, _2, _3, _4) _2 _3[_4];
#define __MEMBER_DEFINE_IMPL_2(Type, _2, _3, _4) Type<_2> _3;
#define __MEMBER_DEFINE_IMPL_3(Type, _2, _3, _4) Type<_2> _3[_4];
#define __MEMBER_DEFINE(Type, _1, _2, _3, _4) __MEMBER_DEFINE_IMPL_##_1(Type, _2, _3, _4)

#define __MEMBER_VISIT(Type, _1, _2, _3, _4) _visit_impl(fn, _3);

#define DECLARE_META_TYPE(Name, EnumT) template<typename T> using Name = PropertyMeta<EnumT, T>;

#define SUB_TYPE(Type, Name) 0, Type, Name, 0
#define SUB_TYPE_ARRAY(Type, Name, Size) 1, Type, Name, Size

#define MEMBER(Type, Name) 2, Type, Name, 0
#define MEMBER_ARRAY(Type, Name, Size) 3, Type, Name, Size


#define DECLARE_META(TypeName, MetaType, ...)                               \
struct TypeName {                                                           \
    FOR_EACH_OPTS_4(__MEMBER_DEFINE, MetaType, __VA_ARGS__)                 \
                                                                            \
    void visit(const MetaVisitFn &fn) {                                     \
        FOR_EACH_OPTS_4(__MEMBER_VISIT, MetaType, __VA_ARGS__)              \
    }                                                                       \
};
