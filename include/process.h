#ifndef PROCESS_H
#define PROCESS_H

#include <inttypes.h>
#include <stdbool.h>

/**
 * @brief Initializes the process module
 *
 * @param is_primary whether to initialize the primary or backup
 * @param index elevator index
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int process_init(bool is_primary, size_t index);

#endif
