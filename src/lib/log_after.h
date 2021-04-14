
#pragma once

inline std::mutex printf_lock;

inline void log(literal fmt, ...) {
    std::lock_guard lock(printf_lock);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt.str(), args);
    va_end(args);
    fflush(stdout);
}
