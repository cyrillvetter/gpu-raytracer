#pragma once

#include <cstdint>

#define NON_COPYABLE_DEFAULT_MOVABLE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator=(ClassName&&) noexcept = default;

#define NON_COPYABLE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete;

#define FLAG_OPERATORS(FlagName) \
    constexpr FlagName operator|(FlagName a, FlagName b) { \
        return static_cast<FlagName>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); \
    } \
    constexpr FlagName operator&(FlagName a, FlagName b) { \
        return static_cast<FlagName>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); \
    } \
    constexpr FlagName& operator|=(FlagName& a, FlagName b) { \
        a = a | b; \
        return a; \
    } \
    constexpr FlagName& operator&=(FlagName& a, FlagName b) { \
        a = a & b; \
        return a; \
    };

typedef uint64_t BufferSize;

struct Dimensions final {
    int width;
    int height;

    Dimensions() = delete;
    Dimensions(int width, int height) : width(width), height(height) {}
};
