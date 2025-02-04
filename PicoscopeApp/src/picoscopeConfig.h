#include <unistd.h>

#ifndef PICOSCOPE_CONFIG
#define PICOSCOPE_CONFIG

enum Channel{
    CHANNEL_A = 0,
    CHANNEL_B = 1,
    CHANNEL_C = 2,
    CHANNEL_D = 3,
    CHANNEL_E = 4,
};

enum RatioMode {
    RATIO_MODE_AGGREGATE = 1,
    RATIO_MODE_DECIMATE = 2,
    RATIO_MODE_AVERAGE = 4,
    RATIO_MODE_TRIGGER_DATA_FOR_TIME_CALCULATION = 0x10000000, 
    RATIO_MODE_TRIGGER = 0x40000000, 
    RATIO_MODE_RAW = 0x80000000
};

/** Structure for channel configurations  */
struct ChannelConfigs{
    enum Channel channel;
    int16_t coupling; 
    int16_t range; 
    double analogue_offset; 
    int16_t bandwidth;
};

struct SampleConfigs{ 
    u_int32_t timebase; 
    u_int64_t num_samples; 
    u_int64_t pre_trigger_samples; 
    u_int64_t post_trigger_samples; 
    u_int64_t down_sample_ratio;
    enum RatioMode down_sample_ratio_mode; 
};

/** Get the channel from the record formatted "OSCXXXX-XX:CH[A-B]:" and return index in channels array */
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs* channels[]);

#endif