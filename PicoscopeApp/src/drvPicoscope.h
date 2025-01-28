#include "PicoStatus.h"
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

int16_t get_serial_num(int8_t** serial_num_return);
int connect_picoscope();
#endif
