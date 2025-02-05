#include "picoscopeConfig.h"
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

int16_t get_serial_num(int8_t** serial_num_return);

int16_t connect_picoscope();

int16_t open_picoscope(int16_t resolution); 

int16_t set_device_resolution(int16_t resolution);

int16_t close_picoscope();

int16_t set_channel_on(struct ChannelConfigs* channel);

int16_t set_channel_off(int channel);

int16_t retrieve_waveform(
    struct ChannelConfigs* channel_configuration,
    struct SampleConfigs* sample_configurations,
    int16_t* waveform);

#endif
