#include "picoscopeConfig.h"
#include <epicsEvent.h>
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

typedef struct PS6000AModule {
    char* serial_num;
    struct SampleConfigs sample_config;
    struct ChannelConfigs channel_configs[CHANNEL_NUM];
    struct TriggerConfigs trigger_config;
    uint64_t sample_collected;
	epicsEventId triggerReadyEvent;
}PS6000AModule;

PS6000AModule* PS6000ACreateModule(char* serial_num);

PS6000AModule* PS6000AGetModule(char* serial_num);

uint32_t get_device_info(int8_t** device_info);

uint32_t open_picoscope(int16_t resolution, int8_t* serial_num); 

uint32_t ping_picoscope();

uint32_t set_device_resolution(int16_t resolution);

uint32_t get_resolution(int16_t* resolution);

uint32_t close_picoscope();

uint32_t set_channel_on(struct ChannelConfigs* channel);

uint32_t set_channel_off(int channel);

uint32_t get_channel_status(int16_t channel);


uint32_t get_valid_timebase_configs(
    struct TimebaseConfigs timebase_configs, 
    uint64_t num_samples,   
    double* sample_interval,
    uint32_t* timebase, 
    double* sample_rate
    );  

uint32_t get_analog_offset_limits(
    int16_t range, 
    int16_t coupling, 
    double* max_analog_offset,
    double* min_analog_offset);

uint32_t is_Channel_On(enum Channel channel);

uint32_t setup_picoscope(
    struct PS6000AModule* mp,
    int16_t* waveform_buffer[CHANNEL_NUM]
    );

// uint32_t interrupt_block_capture();

uint32_t run_block_capture(
    struct PS6000AModule* mp,
    double* time_indisposed_ms
    );

uint32_t get_analogue_offset_limits(
    int16_t range, 
    int16_t coupling, 
    double* max_analogue_offset,
    double* min_analogue_offset
    );

uint32_t validate_sample_interval(
    double requested_time_interval, 
    uint32_t* timebase, 
    double* available_time_interval
    );

uint32_t stop_capturing();


#endif
