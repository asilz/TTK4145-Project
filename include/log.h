#include <stdio.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#if LOG_LEVEL > 2
#define LOG_INFO(...) printf(__VA_ARGS__);
#else
#define LOG_INFO(...)
#endif

#if LOG_LEVEL > 1
#define LOG_WARNING(...) printf(__VA_ARGS__);
#else
#define LOG_WARNING(...)
#endif

#if LOG_LEVEL > 0
#define LOG_ERROR(...) printf(__VA_ARGS__);
#else
#define LOG_ERROR(...)
#endif
