
#pragma once

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

inline std::mutex printf_lock;
inline std::ofstream log_file("log.txt");

inline void log(std::string fmt, ...) {
    std::lock_guard<std::mutex> lock(printf_lock);
    static char buffer[1024];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buffer, 1024, fmt.c_str(), args);
    va_end(args);
    if(n < 1023)
        buffer[n] = '\0';
    else
        buffer[1023] = '\0';
    std::cout << std::string(buffer);
    log_file << std::string(buffer);
    std::cout.flush();
    log_file.flush();
}

inline std::string last_file(std::string path) {
    size_t p = path.find_last_of("\\/") + 1;
    return path.substr(p, path.size() - p);
}

/// Log informational message
#define info(fmt, ...)                                                                             \
    (void)(log("%s:%u [info] " fmt "\n", last_file(__FILE__).c_str(), __LINE__, ##__VA_ARGS__))

/// Log warning (red)
#define warn(fmt, ...)                                                                             \
    (void)(log("\033[0;31m%s:%u [warn] " fmt "\033[0m\n", last_file(__FILE__).c_str(), __LINE__,   \
               ##__VA_ARGS__))

/// Log fatal error and exit program
#define die(fmt, ...)                                                                              \
    (void)(log("\033[0;31m%s:%u [fatal] " fmt "\033[0m\n", last_file(__FILE__).c_str(), __LINE__,  \
               ##__VA_ARGS__),                                                                     \
           std::exit(__LINE__));

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#elif defined(__GNUC__)
#define DEBUG_BREAK __builtin_trap()
#elif defined(__clang__)
#define DEBUG_BREAK __builtin_debugtrap()
#else
#error Unsupported compiler.
#endif

#define fail_assert(msg, file, line)                                                               \
    (void)(log("\033[1;31m%s:%u [ASSERT] " msg "\033[0m\n", file, line), DEBUG_BREAK,              \
           std::exit(__LINE__), 0)

#undef assert
#define assert(...)                                                                                \
    (void)((!!(__VA_ARGS__)) ||                                                                    \
           (fail_assert(#__VA_ARGS__, last_file(__FILE__).c_str(), __LINE__), 0))
