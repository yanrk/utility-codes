#ifndef STUCK_CHECK_H
#define STUCK_CHECK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

int open_stuck_checker();
void close_stuck_checker();
int stuck_set_device(const char *serial_number, const char *user_key);
int stuck_checker_in(const char *file, const char *func, int line, uint32_t timeout);
int stuck_checker_out();
int stuck_delay_reset(uint32_t timeout);
int stuck_delay_clear();

#ifdef __cplusplus
}
#endif


#endif // STUCK_CHECK_H
