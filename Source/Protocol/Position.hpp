#pragma once

#include <cstdint>

namespace Position {
    inline int64_t encode(int32_t x, int32_t y, int32_t z) {
        return ((static_cast<int64_t>(x) & 0x3FFFFFF) << 38)
            | ((static_cast<int64_t>(y) & 0xFFF) << 26)
            | (static_cast<int64_t>(z) & 0x3FFFFFF);
    }

    inline void decode(int64_t value, int32_t& x, int32_t& y, int32_t& z) {
        x = static_cast<int32_t>(value >> 38);
        y = static_cast<int32_t>((value >> 26) & 0xFFF);
        z = static_cast<int32_t>((value << 38) >> 38);

        if (x >= (1 << 25)) x -= (1 << 26);
        if (y >= (1 << 11)) y -= (1 << 12);
        if (z >= (1 << 25)) z -= (1 << 26);
    }
}