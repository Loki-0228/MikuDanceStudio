#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <cerrno>
namespace mikudancestudio::frame_limits {
// The editor and native/plugin records share signed 32-bit frame indices.
// Reserve the final value for exclusive range ends and the next-frame cursor.
inline constexpr std::int32_t maximum = (std::numeric_limits<std::int32_t>::max)() - 1;
inline constexpr std::int32_t legacyMaximum = 10000000;
inline bool Add(std::int64_t offset, std::uint64_t frame, std::uint32_t& result) noexcept {
    if(offset<0 || frame>std::uint64_t(maximum) || std::uint64_t(offset)>std::uint64_t(maximum)-frame)return false;
    result=static_cast<std::uint32_t>(std::uint64_t(offset)+frame);return true;
}
inline bool Convert(double value, std::int32_t& result) noexcept {
    if(!std::isfinite(value)||value<0||value>maximum)return false;
    result=static_cast<std::int32_t>(value);return true;
}
inline bool Parse(const char* value,std::int32_t& result) noexcept {
    if(!value||!*value)return false;
    errno=0;char* end=nullptr;const auto parsed=std::strtoll(value,&end,10);
    if(errno==ERANGE||end==value||*end||parsed<0||parsed>maximum)return false;
    result=static_cast<std::int32_t>(parsed);return true;
}
}
