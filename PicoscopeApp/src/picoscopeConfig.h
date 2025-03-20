#include <unistd.h>
#include <stdint.h>

#ifndef PICOSCOPE_CONFIG
#define PICOSCOPE_CONFIG

#define CHANNEL_NUM 4

enum Channel{
    CHANNEL_A = 0,
    CHANNEL_B = 1,
    CHANNEL_C = 2,
    CHANNEL_D = 3,
    TRIGGER_AUX = 1001, 
    NO_CHANNEL = 10
};
enum ThresholdDirection
{
  ABOVE = 0, //using upper threshold
  BELOW = 1, //using upper threshold
  RISING = 2, // using upper threshold
  FALLING = 3, // using upper threshold
  RISING_OR_FALLING = 4, // using both thresholds
  ABOVE_LOWER = 5, // using lower threshold
  BELOW_LOWER = 6, // using lower threshold
  RISING_LOWER = 7, // using lower threshold
  FALLING_LOWER = 8, // using lower threshold

  // Windowing using both thresholds
  INSIDE = ABOVE,
  OUTSIDE = BELOW,
  ENTER = RISING,
  EXIT = FALLING,
  ENTER_OR_EXIT = RISING_OR_FALLING,
  POSITIVE_RUNT = 9,
  NEGATIVE_RUNT = 10,

  // no trigger set
  NONE = RISING
};

enum ThresholdMode
{
  LEVEL = 0,
  WINDOW = 1
};

enum TriggerType {
    NO_TRIGGER = 0, 
    SIMPLE_EDGE = 1,
}; 

enum RatioMode {
    RATIO_MODE_AGGREGATE = 1,
    RATIO_MODE_DECIMATE = 2,
    RATIO_MODE_AVERAGE = 4,
    RATIO_MODE_TRIGGER_DATA_FOR_TIME_CALCULATION = 0x10000000, 
    RATIO_MODE_TRIGGER = 0x40000000, 
    RATIO_MODE_RAW = 0x80000000
};

enum UnitPerDiv {
    ns_per_div = 0, 
    us_per_div = 1, 
    ms_per_div = 2, 
    s_per_div = 3
};

/** Structure for channel configurations  */
struct ChannelConfigs{
    enum Channel channel;
    int16_t coupling; 
    int16_t range; 
    double analog_offset; 
    int32_t bandwidth;
};


/** Structure for data capture configurations*/
struct SampleConfigs{ 
    uint64_t num_samples; 
    float trigger_position_ratio;
    uint64_t down_sample_ratio;
    enum RatioMode down_sample_ratio_mode; 
    
    struct TimebaseConfigs {
        int16_t num_divisions;
        enum UnitPerDiv time_per_division_unit; 
        double time_per_division;
        uint32_t timebase;
        double sample_interval_secs;
        double sample_rate; 
    } timebase_configs; 
};

/** Structure for data trigger configurations*/
struct TriggerConfigs{ 
    enum TriggerType triggerType;
    enum Channel channel;
    enum ThresholdDirection thresholdDirection;
    enum ThresholdMode thresholdMode;
    int16_t thresholdUpper;
    int16_t thresholdLower;
    uint32_t autoTriggerMicroSeconds;
};

/** Get the channel from the record formatted "OSCXXXX-XX:CH[A-B]:" and return index in channels array */
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs* channels[]);

#endif
