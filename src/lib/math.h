
#pragma once

#include <cmath>

#ifdef _WIN32
#include <intrin.h>
#endif

#define PI32 3.14159265358979323846264338327950288f
#define PI64 3.14159265358979323846264338327950288

#define _MAX(a, b) ((a) > (b) ? (a) : (b))
#define _MIN(a, b) ((a) < (b) ? (a) : (b))

#define Radians(v) (v * (PI32 / 180.0f))
#define Degrees(v) (v * (180.0f / PI32))

#define KB(x) (1024 * (x))
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

inline u32 prev_pow2(u32 val) {

    u32 pos = 0;
#ifdef _MSC_VER
    _BitScanReverse((unsigned long*)&pos, val);
#else
    for(u32 bit = 31; true; bit--) {
        if(val & (1 << bit)) {
            pos = bit;
            break;
        }
        if(bit == 0) {
            return 0;
        }
    }
#endif
    return 1 << pos;
}

inline u32 ceil_pow2(u32 x) {
    u32 prev = prev_pow2(x);
    return x == prev ? x : prev << 1;
}

template<typename T> T lerp(T min, T max, T t) {
    return min + (max - min) * t;
}

template<typename T> T clamp(T x, T min, T max) {
    return _MIN(_MAX(x, min), max);
}

inline f32 frac(f32 x) {
    return x - (i64)x;
}

inline f32 smoothstep(f32 e0, f32 e1, f32 x) {
    f32 t = clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
