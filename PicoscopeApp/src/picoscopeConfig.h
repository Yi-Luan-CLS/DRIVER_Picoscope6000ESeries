#include <unistd.h>

#include "PicoDeviceEnums.h"

#ifndef PICOSCOPE_CONFIG
#define PICOSCOPE_CONFIG

/** For converting PV name to channel enum */
enum enPicoChannel record_name_to_pico_channel(const char* channel_str);

/** Structure for channel configurations  */
struct channel_configurations{
    enum enPicoChannel channel;
    int16_t coupling; 
    int16_t range; 
    double analogue_offset; 
    int16_t bandwidth;
};
#endif