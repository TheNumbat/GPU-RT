
#pragma once

inline void log(literal fmt, ...);

#define info(fmt, ...)                                                                             \
    (void)(log("%s:%u [info] %s\n", last_file(literal(__FILE__)).str(), __LINE__,                  \
               scratch_format(fmt, ##__VA_ARGS__).str()),                                          \
           0)

#define warn(fmt, ...)                                                                             \
    (void)(log("\033[0;31m%s:%u [warn] %s\033[0m\n", last_file(literal(__FILE__)).str(), __LINE__, \
               scratch_format(fmt, ##__VA_ARGS__).str()),                                          \
           0)

#ifdef _WIN32
#define DEBUG_BREAK __debugbreak()
#elif defined(__linux__)
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP)
#endif

#define die(fmt, ...)                                                                              \
    (void)(log("\033[1;31m%s:%u [FATAL] %s\033[0m\n", last_file(literal(__FILE__)).str(),          \
               __LINE__, scratch_format(fmt, ##__VA_ARGS__).str()),                                \
           DEBUG_BREAK, ::exit(__LINE__), 0)

#define fail_assert(msg, file, line)                                                               \
    (void)(log("\033[1;31m%s:%u [ASSERT] " msg "\033[0m\n", file, line), DEBUG_BREAK,              \
           ::exit(__LINE__), 0)

#undef assert
#define assert(expr)                                                                               \
    (void)((!!(expr)) || (fail_assert(#expr, last_file(literal(__FILE__)).str(), __LINE__), 0))

struct Location {
    literal func, file;
    usize line = 0;
};

inline bool operator==(const Location& l, const Location& r) {
    return l.line == r.line && l.file == r.file && l.func == r.func;
}

#define Here (Location{__func__, last_file(literal(__FILE__)), (usize)__LINE__})
