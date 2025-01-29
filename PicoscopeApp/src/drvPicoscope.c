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

int16_t connect_picoscope(){
    bool open = false;
    int tries = 0;
    while(!open){
        status = ps6000aOpenUnit(&handle, NULL, PICO_DR_12BIT);
        tries ++;
        if (status != PICO_OK && tries <= MAX_CONNECT_TRIES) {
            sleep(5);
        }else{  open = true;    }
    }
    return status;
}


int8_t* serial_num_buffer;
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

    status = ps6000aSetChannelOn(handle, channel->channel, channel->coupling, channel->range, channel->analogue_offset, channel->bandwidth);
    if (status != PICO_OK) 
    {
        printf("Status: %d\n", status);
        return status;
    }
    return status;
}

int set_channel_off(int channel) {

    status = ps6000aSetChannelOff(handle, channel);
    if (status != PICO_OK)
    {
        printf("Status: %d\n", status);
        return status;
    }
    return status;
}