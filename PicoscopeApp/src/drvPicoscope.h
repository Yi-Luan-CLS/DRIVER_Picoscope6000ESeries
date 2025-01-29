#include "PicoStatus.h"
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE


int16_t get_serial_num(int8_t** serial_num_return);

int connect_picoscope();

int16_t set_coupling(struct ChannelConfigs* channel);

int16_t set_channel_on(int16_t channel);

int set_channel_off(int channel);


int16_t get_waveform(int16_t** waveform);
#endif
