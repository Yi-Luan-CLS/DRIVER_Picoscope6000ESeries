#include "picoscopeConfig.h"
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

int16_t get_device_info(int8_t** device_info);

int16_t connect_picoscope();

int16_t open_picoscope(int16_t resolution, int8_t* serial_num); 

int16_t ping_picoscope();

int16_t set_device_resolution(int16_t resolution);

int16_t get_resolution(int16_t* resolution);

int16_t close_picoscope();

int16_t set_channel_on(struct ChannelConfigs* channel);

int16_t set_channel_off(int channel);

// int16_t retrieve_waveform(
//     struct ChannelConfigs* channel_configuration,
//     struct SampleConfigs* sample_configurations,
//     int16_t* waveform);

int16_t setup_picoscope(
    int16_t* waveform_buffer,
    struct ChannelConfigs* channel_config,
    struct SampleConfigs* sample_config,
    struct TriggerConfigs* trigger_config
    );

int16_t run_block_capture(
    struct SampleConfigs* sample_config,
    double* time_indisposed_ms,
    uint8_t* capturing
    );

int16_t get_analogue_offset_limits(
    int16_t range, 
    int16_t coupling, 
    double* max_analogue_offset,
    double* min_analogue_offset
    );

int16_t set_sample_interval(
    double requested_time_interval, 
    uint32_t* timebase, 
    double* available_time_interval
    );

#endif
