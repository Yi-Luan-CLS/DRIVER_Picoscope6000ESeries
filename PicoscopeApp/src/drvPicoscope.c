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

void log_error(char* function_name, int16_t status, const char* FILE, int LINE){ 
    printf("Error in %s (file: %s, line: %d). Status code: 0x%08lX\n", function_name, FILE, LINE, status);
}

int16_t close_picoscope(){ 
    
    status = ps6000aCloseUnit(handle);
    handle = 0;
    if (status != PICO_OK) 
    { 
        log_error("ps6000aCloseUnit", status, __FILE__, __LINE__);
        return -1;  
    }

    return 0;
}

int16_t open_picoscope(int16_t resolution){

    // If handle is less than or equal to zero no device is open.
    // Picoscope API returns handle as -1 if attempt to open device fails.
    if (handle <= 0) {
        status = ps6000aOpenUnit(&handle, NULL, resolution);
        if (status != PICO_OK) 
        { 
            log_error("ps6000aOpenUnit", status, __FILE__, __LINE__); 
            return -1;  
        }
        printf("Open unit with resolution: %d\n", resolution);
    }
    return 0;
}

int16_t set_device_resolution(int16_t resolution){ 
    
    status = ps6000aSetDeviceResolution(handle, resolution); 

    if (status != PICO_OK){ 
        log_error("ps6000aSetDeviceResolution", status, __FILE__, __LINE__);
        return -1;
    }

    return 0; 
}


int16_t get_serial_num(int8_t** serial_num) {
    int16_t required_size;

    status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_BATCH_AND_SERIAL);
    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return -1;  
    }

    if (serial_num_buffer == NULL)
        serial_num_buffer = malloc(required_size);
    memset(serial_num_buffer, 0, required_size);

    status = ps6000aGetUnitInfo(handle, serial_num_buffer, required_size, &required_size, PICO_BATCH_AND_SERIAL);
    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return -1;  
    }

    *serial_num = serial_num_buffer;

    return 0;  
}

int16_t set_channel_on(struct ChannelConfigs* channel) {
    

    status = ps6000aSetChannelOn(handle, channel->channel, channel->coupling, channel->range, channel->analogue_offset, channel->bandwidth);
    if (status != PICO_OK) 
    {
        log_error("ps6000aSetChannelOn", status, __FILE__, __LINE__);
        return -1;
    }

    printf("Set channel %d on.\n", channel->channel);

    printf("Coupling: %d\n", channel->coupling); 
    printf("Range: %d\n", channel->range); 
    printf("Analogue offset: %f\n", channel->analogue_offset);     
    printf("Bandwidth: %d\n", channel->bandwidth); 
  
    return 0;

}

int16_t set_channel_off(int channel) {


    status = ps6000aSetChannelOff(handle, channel);
    if (status != PICO_OK)
    {
        log_error("ps6000aSetChannelOff", status, __FILE__, __LINE__);
        return -1;
    }
    printf("Set channel %d off.\n", channel);
    return 0;
}

// int16_t
// retrieve_waveform(struct ChannelConfigs* channel_configuration,
//                     // struct SampleConfigs* sample_configuration,
//                     int16_t** waveform,
//                     int sample_size){
//     int16_t* waveform_buffer = NULL;
//     double timeIntervalNs = 0.0;
//     uint64_t maxSamples = 0;
//     double timeIndisposedMs = 0;
//     uint32_t timebase = 20 ;
//     int16_t triggerStatus = 0;

//     status = ps6000aSetChannelOn(
//         handle,
//         channel_configuration->channel,
//         channel_configuration->coupling,
//         channel_configuration->range,
//         channel_configuration->analogue_offset,
//         channel_configuration->bandwidth
//         );
//     // sample_configuration->down_sample_ratio;
//     // sample_configuration->down_sample_ratio_mode;
//     // sample_configuration->post_trigger_samples;
//     // sample_configuration->pre_trigger_samples;
//     // sample_configuration->num_samples;

//     waveform_buffer = (int16_t *)malloc(sizeof(int16_t) * sample_size);  // Allocate memory for the captured data
//     memset(waveform_buffer, 0, sizeof(int16_t) * sample_size);
//     status = ps6000aRunBlock(
//         handle,
//         // sample_configuration->pre_trigger_samples,
//         // sample_configuration->post_trigger_samples,
//         0,
//         sample_size,
//         timebase,
//         &timeIndisposedMs,
//         0,
//         NULL, NULL);
//     if(status!=PICO_OK){
//         printf("ps6000aRunBlock Error Code: %d.\n",status);
//         return status;
//     }

//     while (1) {
//             status = ps6000aIsReady(handle, &triggerStatus);
//             if(status!=PICO_OK){
//                 printf("ps6000aIsReady Error Code: %d.\n",status);
//                 break;
//             }
//             if (triggerStatus == 1) {
//                 printf("Capture finished.\n");
//                 break;
//             }
//             usleep(10000);  
//         }

//     status = ps6000aSetDataBuffer(handle, PICO_CHANNEL_B, waveform_buffer, sample_size, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_ADD);
//     if(status!=PICO_OK){
//         printf("ps6000aSetDataBuffer Error Code: %d.\n",status);
//         return status;
//     }
    
//     uint64_t startIndex = 0;
//     uint64_t noOfSamples = sample_size;
//     uint64_t downSampleRatio = 1;
//     PICO_RATIO_MODE downSampleRatioMode = PICO_RATIO_MODE_RAW;
//     uint64_t segmentIndex = 0;
//     int16_t overflow = 0;
    
//     status = ps6000aGetValues(handle, startIndex, &noOfSamples, downSampleRatio, downSampleRatioMode, segmentIndex, &overflow);
//     if(status!=PICO_OK){
//         printf("ps6000aGetValues Error Code: %d.\n",status);
//         return status;
//     }
    
//     *waveform = waveform_buffer;
//     printf("%d \n", waveform_buffer[1]);
//     return status;
 
// }

int16_t configure_channel(struct ChannelConfigs* channel_config);
int16_t allocate_waveform_buffer(int16_t** buffer, struct SampleConfigs* sample_config);
int16_t run_block_capture(struct SampleConfigs* sample_config, uint32_t timebase, double* time_indisposed_ms);
int16_t wait_for_capture_completion();
int16_t set_data_buffer(int16_t* waveform_buffer, struct ChannelConfigs* channel_config, struct SampleConfigs* sample_config);
int16_t retrieve_waveform_data(int16_t* waveform_buffer, struct SampleConfigs* sample_config);

int16_t retrieve_waveform(struct ChannelConfigs* channel_config, 
                          struct SampleConfigs* sample_config, 
                          int16_t* waveform_buffer) {
    int16_t status = 0;
    double time_indisposed_ms = 0;
    uint32_t timebase = 2;

    // status = configure_channel(channel_config);
    // if (status != PICO_OK) {
    //     return status;
    // }

    // status = allocate_waveform_buffer(&waveform_buffer, sample_config);
    // if (status != PICO_OK) {
    //     return status;
    // }

    status = run_block_capture(sample_config, timebase, &time_indisposed_ms);
    if (status != PICO_OK) {
        // free(waveform_buffer);
        return status;
    }

    status = wait_for_capture_completion();
    if (status != PICO_OK) {
        // free(waveform_buffer);
        return status;
    }

    status = set_data_buffer(waveform_buffer, channel_config, sample_config);
    if (status != PICO_OK) {
        // free(waveform_buffer);
        return status;
    }

    status = retrieve_waveform_data(waveform_buffer, sample_config);
    if (status != PICO_OK) {
        // free(waveform_buffer);
        return status;
    }

    // *waveform = waveform_buffer;
    printf("First sample value: %d\n", waveform_buffer[0]);

    return status;
}

// int16_t configure_channel(struct ChannelConfigs* channel_config) {
//     int16_t status = ps6000aSetChannelOn(
//         handle,
//         channel_config->channel,
//         channel_config->coupling,
//         channel_config->range,
//         channel_config->analogue_offset,
//         channel_config->bandwidth
//     );

//     if (status != PICO_OK) {
//         printf("ps6000aSetChannelOn Error Code: %d.\n", status);
//     }

//     return status;
// }

int16_t allocate_waveform_buffer(int16_t** buffer, struct SampleConfigs* sample_config) {
    *buffer = (int16_t*)malloc(sizeof(int16_t) * sample_config->num_samples);
    if (*buffer == NULL) {
        printf("Memory allocation failed for waveform buffer.\n");
        return -1;
    }
    memset(*buffer, 0, sizeof(int16_t) * sample_config->num_samples);
    return 0;
}

int16_t run_block_capture(struct SampleConfigs* sample_config, uint32_t timebase, double* time_indisposed_ms) {
    int16_t status = ps6000aRunBlock(
        handle,
        sample_config->pre_trigger_samples,
        sample_config->post_trigger_samples,
        timebase,
        time_indisposed_ms,
        0,
        NULL, 
        NULL
    );

    if (status != PICO_OK) {
        printf("ps6000aRunBlock Error Code: %d.\n", status);
    }

    return status;
}

int16_t wait_for_capture_completion() {
    int16_t status = 0;
    int16_t trigger_status = 0;

    while (1) {
        status = ps6000aIsReady(handle, &trigger_status);
        if (status != PICO_OK) {
            printf("ps6000aIsReady Error Code: %d.\n", status);
            return status;
        }

        if (trigger_status == 1) {
            printf("Capture finished.\n");
            break;
        }

        usleep(10000);
    }

    return 0;
}

int16_t set_data_buffer(int16_t* waveform_buffer, struct ChannelConfigs* channel_config, struct SampleConfigs* sample_config) {

    int16_t status = ps6000aSetDataBuffer(
        handle, 
        channel_config->channel, 
        waveform_buffer, 
        sample_config->num_samples, 
        PICO_INT16_T, 
        0, 
        sample_config->down_sample_ratio_mode, 
        PICO_ADD
    );

    if (status != PICO_OK) {
        printf("ps6000aSetDataBuffer Error Code: %d.\n", status);
    }

    return status;
}

int16_t retrieve_waveform_data(int16_t* waveform_buffer, struct SampleConfigs* sample_config) {
    uint64_t start_index = 0;
    uint64_t segment_index = 0;
    int16_t overflow = 0;
    int16_t status = ps6000aGetValues(
        handle, 
        start_index, 
        &sample_config->num_samples, 
        sample_config->down_sample_ratio, 
        sample_config->down_sample_ratio_mode, 
        segment_index, 
        &overflow
    );

    if (status != PICO_OK) {
        printf("ps6000aGetValues Error Code: %d.\n", status);
    }

    return status;
}