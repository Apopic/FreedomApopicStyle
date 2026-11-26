#pragma once
#include <cmath>
#include <limits>
#include <type_traits>

namespace numeric {
    // Integer conversions discard the fractional part and saturate at the target range.
    // Check before casting: out-of-range floating-to-integer conversion is undefined.
    template<class To, class From> To Cast(From value) noexcept {
        static_assert(std::is_arithmetic_v<To> && std::is_arithmetic_v<From>);
        const long double x=static_cast<long double>(value);
        if(std::isnan(x)) return To{};
        if(x>=static_cast<long double>(std::numeric_limits<To>::max())) return std::numeric_limits<To>::max();
        if(x<=static_cast<long double>(std::numeric_limits<To>::lowest())) return std::numeric_limits<To>::lowest();
        return static_cast<To>(value);
    }
}
