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

int set_channel_on(struct ChannelConfigs* channel) {
    
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
retrieve_waveform(struct ChannelConfigs* channel_configuration, int16_t** waveform, int sample_size){
    int16_t* waveform_buffer = NULL;
    // MAX_BUFFER_SIZE: 1073741696
    status = ps6000aSetChannelOn(
        handle,
        channel_configuration->channel,
        channel_configuration->coupling,
        channel_configuration->range,
        channel_configuration->analogue_offset,
        channel_configuration->bandwidth
        );    
    status = ps6000aSetChannelOn(handle, PICO_CHANNEL_B, PICO_DC, PICO_X1_PROBE_20V, 0.0, PICO_BW_FULL);
    double timeIntervalNs = 0.0;
    uint64_t maxSamples = 0;
    double timeIndisposedMs = 0;
    uint32_t timebase = 1 ;
    int16_t triggerStatus = 0;
    waveform_buffer = (int16_t *)malloc(sizeof(int16_t) * sample_size);  // Allocate memory for the captured data
    memset(waveform_buffer, 0, sizeof(int16_t) * sample_size);

    status = ps6000aRunBlock(handle, 0, sample_size, timebase, &timeIndisposedMs, 0, NULL, NULL);
    if(status!=PICO_OK){
        printf("ps6000aRunBlock Error Code: %d.\n",status);
    }

    while (1) {
            status = ps6000aIsReady(handle, &triggerStatus);
            if(status!=PICO_OK){
                printf("ps6000aIsReady Error Code: %d.\n",status);
                break;
            }
            if (triggerStatus == 1) {
                printf("Capture finished.\n");
                break;
            }
            usleep(10000);  
        }

    status = ps6000aSetDataBuffer(handle, PICO_CHANNEL_B, waveform_buffer, sample_size, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_ADD);
    if(status!=PICO_OK){
        printf("ps6000aSetDataBuffer Error Code: %d.\n",status);
        return status;
    }
    
    uint64_t startIndex = 0;
    uint64_t noOfSamples = sample_size;
    uint64_t downSampleRatio = 1;
    PICO_RATIO_MODE downSampleRatioMode = PICO_RATIO_MODE_RAW;
    uint64_t segmentIndex = 0;
    int16_t overflow = 0;
    
    status = ps6000aGetValues(handle, startIndex, &noOfSamples, downSampleRatio, downSampleRatioMode, segmentIndex, &overflow);
    if(status!=PICO_OK){
        printf("ps6000aGetValues Error Code: %d.\n",status);
        return status;
    }
    
    *waveform = waveform_buffer;
    printf("%d \n", waveform_buffer[1]);
    return status;
 
}