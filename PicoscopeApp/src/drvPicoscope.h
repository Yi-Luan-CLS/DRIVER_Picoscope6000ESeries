#include "PicoStatus.h"
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

int16_t get_serial_num(int8_t** serial_num_return);

int connect_picoscope();

int16_t set_coupling(int16_t coupling);

int16_t set_channel_on(int16_t channel);

int set_channel_off(int channel);


/** For converting PV name to channel enum */
// Enum for Pico Channel types
enum PicoChannel {
    PICO_CHANNEL_A,  // Enum value for CHA
    PICO_CHANNEL_B,  // Enum value for CHB
    PICO_CHANNEL_C,  // Enum value for CHC
    PICO_CHANNEL_D   // Enum value for CHD
};

enum PicoChannel record_name_to_pico_channel(const char* channel_str);

#endif
