#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#if LOG_LEVEL > 2
#define LOG_INFO__(...) printf(__VA_ARGS__)
#else
#define LOG_INFO__(...)
#endif

#if LOG_LEVEL > 1
#define LOG_WARNING__(...) printf(__VA_ARGS__)
#else
#define LOG_WARNING__(...)
#endif

#if LOG_LEVEL > 0
#define LOG_ERROR__(...) printf(__VA_ARGS__)
#else
#define LOG_ERROR__(...)
#endif

/**
 * @brief Writes an INFO level message to the log
 *
 * @param ... A string optionally containing printf valid conversion specifier, followed by as many values as specifiers
 */
#define LOG_INFO(...) LOG_INFO__(__VA_ARGS__)
/**
 * @brief Writes an WARNING level message to the log
 *
 * @param ... A string optionally containing printf valid conversion specifier, followed by as many values as specifiers
 */
#define LOG_WARNING(...) LOG_WARNING__(__VA_ARGS__)
/**
 * @brief Writes an ERROR level message to the log
 *
 * @param ... A string optionally containing printf valid conversion specifier, followed by as many values as specifiers
 */
#define LOG_ERROR(...) LOG_ERROR__(__VA_ARGS__)

#endif
