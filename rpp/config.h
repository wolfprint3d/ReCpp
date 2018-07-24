#pragma once 

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI //__declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

#ifndef EXTERNC
#  ifdef __cplusplus
#    define EXTERNC extern "C"
#  else
#    define EXTERNC
#  endif
#endif

#ifndef RPPCAPI
#  define RPPCAPI EXTERNC RPPAPI
#endif

#ifndef RPP_HAS_CXX17
#  if _MSC_VER
#    define RPP_HAS_CXX17 _HAS_CXX17
#  else
#    define RPP_HAS_CXX17 __cplusplus >= 201703L
#  endif
#endif

#ifndef INLINE_STATIC
#  if RPP_HAS_CXX17
#    define INLINE_STATIC inline static
#  else
#    define INLINE_STATIC static
#  endif
#endif

//// @note Some functions get inlined too aggressively, leading to some serious code bloat
////       Need to hint the compiler to take it easy ^_^'
#ifndef NOINLINE
#  ifdef _MSC_VER
#    define NOINLINE __declspec(noinline)
#  else
#    define NOINLINE __attribute__((noinline))
#  endif
#endif

//// @note Some strong hints that some functions are merely wrappers, so should be forced inline
#ifndef FINLINE
#  ifdef _MSC_VER
#    define FINLINE __forceinline
#  elif __APPLE__
#    define FINLINE inline __attribute__((always_inline))
#  else
#    define FINLINE __attribute__((always_inline))
#  endif
#endif

#if INTPTR_MAX == INT64_MAX
#  define RPP_64BIT 1
#endif

#ifdef _LIBCPP_STD_VER
#  define _HAS_STD_BYTE (_LIBCPP_STD_VER > 16)
#elif !defined(_HAS_STD_BYTE)
#  define _HAS_STD_BYTE 0
#endif

#ifndef NOCOPY_NOMOVE
#define NOCOPY_NOMOVE(T) \
    T(T&& fwd)             = delete; \
    T& operator=(T&& fwd)  = delete; \
    T(const T&)            = delete; \
    T& operator=(const T&) = delete;
#endif

#ifndef NODISCARD
#  if __clang__
#    if __clang_major__ >= 4 || (__clang_major__ == 3 && __clang_minor__ == 9) // since 3.9
#      define NODISCARD [[nodiscard]]
#    else
#      define NODISCARD // not supported in clang <= 3.8
#    endif
#  else
#    define NODISCARD [[nodiscard]]
#  endif
#endif

namespace rpp
{
    #ifndef RPP_BASIC_INTEGER_TYPEDEFS
    #define RPP_BASIC_INTEGER_TYPEDEFS
        using byte   = unsigned char;
        using ushort = unsigned short;
        using uint   = unsigned int;
        using ulong  = unsigned long;
        using int64  = long long;
        using uint64 = unsigned long long;
    #endif

    #ifndef RPP_MINMAX_DEFINED
    #define RPP_MINMAX_DEFINED
        template<class T> T max(T a, T b) { return a > b ? a : b; }
        template<class T> T min(T a, T b) { return a < b ? a : b; }
        template<class T> T min3(T a, T b, T c) { return a < b ? (a<c?a:c) : (b<c?b:c); }
    #endif
}
