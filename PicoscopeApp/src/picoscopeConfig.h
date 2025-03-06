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
    TRIGGER_AUX = 1001
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
    double analog_offset; 
    int16_t bandwidth;
};

/** Structure for data capture configurations*/
struct SampleConfigs{ 
    uint32_t timebase;
    double time_interval_secs;
    uint64_t num_samples; 
    float trigger_position_ratio;
    uint64_t down_sample_ratio;
    enum RatioMode down_sample_ratio_mode; 
};

/** Structure for data trigger configurations*/
struct TriggerConfigs{ 
    enum Channel channel;
    enum ThresholdDirection thresholdDirection;
    enum ThresholdMode thresholdMode;
    int16_t thresholdUpper;
    int16_t thresholdLower;
    uint32_t autoTriggerMicroSeconds;
};

/** Get the channel from the record formatted "OSCXXXX-XX:CH[A-B]:" and return index in channels array */
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs* channels[]);

int16_t translate_down_sample_ratio_mode(int mode); 

int16_t translate_resolution(int mode);

#endif
