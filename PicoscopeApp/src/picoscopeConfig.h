#include <unistd.h>

#include "PicoDeviceEnums.h"

#ifndef PICOSCOPE_CONFIG
#define PICOSCOPE_CONFIG


/** Structure for channel configurations  */
struct ChannelConfigs{
    enum enPicoChannel channel;
    int16_t coupling; 
    int16_t range; 
    double analogue_offset; 
    int16_t bandwidth;
};

struct SampleConfigs{ 
    u_int64_t num_samples; 
    u_int64_t pre_trigger_samples; 
    u_int64_t post_trigger_samples; 
    enum enPicoTimeUnits time_units; 
    u_int64_t down_sample_ratio;
    enum enPicoRatioMode down_sample_ratio_mode; 
};

/** Get the channel from the record formatted "OSCXXXX-XX:CH[A-B]:" and return index in channels array */
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs* channels[]);

#endif