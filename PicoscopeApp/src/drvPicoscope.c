#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "ps6000aApi.h"
#include "PicoStatus.h"

#include "picoscopeConfig.h"

int16_t handle = 0;
int MAX_CONNECT_TRIES = 12;
int16_t status;
int8_t* serial_num_buffer;

int16_t close_picoscope(){ 
    printf("CLOSE UNIT\n");

    status = ps6000aCloseUnit(handle);
    handle = 0;
    if (status != PICO_OK) 
    { 
        printf("Status: %d\n", status);
        return status;  
    }
    printf("HANDLE: %d\n", handle);

    return status;
}


int16_t open_picoscope(int16_t resolution){
    printf("Open unit with resolution: %d\n", resolution);

    // If no picoscope is open, open it. Otherwise close picoscope and 
    // reopen with new configs.
    if (handle == 0) {
        status = ps6000aOpenUnit(&handle, NULL, resolution);
    } else {
        status = close_picoscope(); 
        status = ps6000aOpenUnit(&handle, NULL, resolution);
    }

    if (status != PICO_OK) 
    { 
        printf("Status: %d\n", status);
        return status;  
    }

    return status;
}


int16_t
get_serial_num(int8_t** serial_num) {
    int16_t required_size;

    status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_BATCH_AND_SERIAL);
    if (status != PICO_OK) 
        return status;  

    if (serial_num_buffer == NULL)
        serial_num_buffer = malloc(required_size);
    memset(serial_num_buffer, 0, required_size);

    status = ps6000aGetUnitInfo(handle, serial_num_buffer, required_size, &required_size, PICO_BATCH_AND_SERIAL);

    *serial_num = serial_num_buffer;

    return status;  
}

int set_channel_on(struct channel_configurations* channel) {
    
    printf("Set channel on: %d\n", channel->channel);

    status = ps6000aSetChannelOn(handle, channel->channel, channel->coupling, channel->range, channel->analogue_offset, channel->bandwidth);
    if (status != PICO_OK) 
    {
        printf("Status: %d\n", status);
        return status;
    }
    return status;
}

int set_channel_off(int channel) {

    printf("Set channel off: %d\n", channel);

    status = ps6000aSetChannelOff(handle, channel);
    if (status != PICO_OK)
    {
        printf("Status: %d\n", status);
        return status;
    }
    return status;
}

int16_t
get_waveform(int16_t** waveform){
    int16_t* waveform_buffer;
    waveform_buffer = malloc(sizeof(int16_t)*10);
    *waveform = waveform_buffer;
    return 0;
}