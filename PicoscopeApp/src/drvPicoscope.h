#include "PicoStatus.h"
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

int16_t get_serial_num(int8_t** serial_num_return);

int connect_picoscope();

int16_t set_coupling(int16_t coupling);

int16_t retrieve_waveform(struct ChannelConfigs* channel_configuration, int16_t** waveform);

int set_channel_on(int channel);

int set_channel_off(int channel);
#endif
