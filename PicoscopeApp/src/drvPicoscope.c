#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "ps6000aApi.h"
#include "PicoStatus.h"

#include "picoscopeConfig.h"


int16_t handle = 0; // The identifier of the connected picoscope

int16_t status;

void log_error(char* function_name, int16_t status, const char* FILE, int LINE){ 
    printf("Error in %s (file: %s, line: %d). Status code: 0x%08X\n", function_name, FILE, LINE, status);
}

/**
 * Opens the PicoScope device with the specified resolution. 
 * 
 * @param resolution The sampling resolution to be used when opening the PicoScope.  
 *                   The following values are valid: 
 *                      - 0: 8-bit resolution
 *                      - 10: 10-bit resolution
 *                      - 1: 12-bit resolution
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t open_picoscope(int16_t resolution, int8_t* serial_num){

    status = ps6000aOpenUnit(&handle, serial_num, resolution);
    if (status != PICO_OK) 
    { 
        log_error("ps6000aOpenUnit", status, __FILE__, __LINE__); 
        return -1;  
    }
    
    return 0;
}

/**
 * Close an open PicoScope device.
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t close_picoscope(){ 
    
    status = ps6000aCloseUnit(handle);
    if (status != PICO_OK) 
    { 
        log_error("ps6000aCloseUnit", status, __FILE__, __LINE__);
        return -1;  
    } 
    handle = 0;
    
    return 0;
}

/**
 * Check that open device is still connected. 
 * 
 * @return 1 if the device is connected, 0 if not. 
*/
int16_t ping_count = 0; 
int16_t ping_picoscope(){ 
    
    status = ps6000aPingUnit(handle); 
    if (status != PICO_OK) {
        log_error("ps6000aPingUnit. Cannot ping device.", status, __FILE__, __LINE__);
        
        // If another call to the driver is made at the same time as this, the error code
        // 0x00000043 will be thrown. The following is to prevent the handle from being reset 
        // to 0 until the ping has failed twice, ensuring the device is actually disconnected.
        if (ping_count >= 2){ 
            handle = 0; // set handle to 0, no device is connected
            ping_count = 0; 
        } else {
            ping_count++; 
        }
        return 0; 
    }
    return 1;
}

/**
 * Set the sample resolution of the currently connected PicoScope. 
 * 
 * @param resolution The sampling resolution to be used.  
 *                   The following values are valid: 
 *                      - 0: 8-bit resolution
 *                      - 10: 10-bit resolution
 *                      - 1: 12-bit resolution
 * 
 * @return 0 if resolution successfully set, or -1 if an error occurs. 
*/
int16_t set_device_resolution(int16_t resolution){ 
    status = ps6000aSetDeviceResolution(handle, resolution); 

    if (status != PICO_OK){ 
        log_error("ps6000aSetDeviceResolution", status, __FILE__, __LINE__);
        return -1;
    }

    return 0; 
}

/**
 * Get  the sample resolution of the currently connected PicoScope. 
 * 
 * @param on exit, the resolution of the device
 * 
 * @return 0 if resolution returned, or -1 if an error occurs. 
*/
PICO_DEVICE_RESOLUTION device_resolution; 
int16_t get_resolution(int16_t* resolution) {


    status = ps6000aGetDeviceResolution(handle, &device_resolution); 

    if(status != PICO_OK) {
        log_error("ps6000aGetDeviceResolution", status, __FILE__, __LINE__);
        return -1;
    }

    *resolution = (int16_t)device_resolution; 
    return 0; 
}

int16_t required_size;
/**
 * Retrieves the serial number of the currently connected PicoScope. 
 * 
 * @param serial_num A pointer to a pointer that will hold the dynamically allocated address of
 *                  the memory buffer containing the serial number. 
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t get_serial_num(int8_t** serial_num) {

    int8_t* serial_num_buffer = NULL;
    
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


/**
 * Retrieves the model number of the currently connected PicoScope. 
 * 
 * @param model_num A pointer to a pointer that will hold the dynamically allocated address of
 *                  the memory buffer containing the model number. 
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t get_model_num(int8_t** model_num) {

    int8_t* model_num_buffer = NULL;
    
    status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_VARIANT_INFO);
    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return -1;  
    }

    if (model_num_buffer == NULL)
        model_num_buffer = malloc(required_size);
    memset(model_num_buffer, 0, required_size);

    status = ps6000aGetUnitInfo(handle, model_num_buffer, required_size, &required_size, PICO_VARIANT_INFO);
    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return -1;  
    }

    *model_num = model_num_buffer;

    return 0;
}


/**
 * Retrieves the model and serial number of the currently connected PicoScope. 
 * 
 * @param device_info A pointer to a pointer that will hold the dynamically allocated address of
 *                    the memory buffer containing the device information. 
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t get_device_info(int8_t** device_info) {

    int8_t* device_info_buffer = NULL; 
	int8_t* serial_num = NULL;
    int8_t* model_num = NULL; 

    status = get_serial_num(&serial_num);
    if (status != 0) {
        return -1;
    }
    status = get_model_num(&model_num); 
    if (status != 0) {
        return -1;
    }
    
    int16_t required_size = strlen((const char*)serial_num) + strlen((const char*)model_num) + 20; 

    if (device_info_buffer == NULL)
        device_info_buffer = malloc(required_size);
    memset(device_info_buffer, 0, required_size);

    snprintf((char*)device_info_buffer, required_size, "Picoscope %s [%s]", (char*)model_num, (char*)serial_num);

    *device_info = device_info_buffer;

    return 0; 
}


// The following struct is intended to track which channels 
// are enabled (1) or disabled (0) using individual bits. 
// This is needed for some function calls to the picoscope API. 
typedef struct {
    uint32_t channel_a : 1;
    uint32_t channel_b : 1;
    uint32_t channel_c : 1;
    uint32_t channel_d : 1;
} EnabledChannelFlags; 

EnabledChannelFlags channel_status = {0}; 

/**
 * Enables a specified channel on the connected Picocope with the given configurations. 
 * Setting the channels coupling, range, analogue offset, and bandwidth. 
 * 
 * @param channel A pointer to a `ChannelConfigs` structure that contains the configuration 
 *                to be activated. The structure holds the coupling type, voltage range, analogue
 *                offset, and bandwidth to configure the channel. 
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t set_channel_on(struct ChannelConfigs* channel) {
    
    status = ps6000aSetChannelOn(handle, channel->channel, channel->coupling, channel->range, channel->analogue_offset, channel->bandwidth);
    if (status != PICO_OK) 
    {
        log_error("ps6000aSetChannelOn", status, __FILE__, __LINE__);
        return -1;
    }

    if (channel->channel == CHANNEL_A) {
        channel_status.channel_a = 1;
    }
    if (channel->channel == CHANNEL_B) {
        channel_status.channel_b = 1;
    }    
    if (channel->channel == CHANNEL_C) {
        channel_status.channel_c = 1;
    }    
    if (channel->channel == CHANNEL_D) {
        channel_status.channel_d = 1;
    }

    printf("Setting channel %d on.\n", channel->channel);
  
    return 0;

}

/**
 * Deactivates the specified channel on the connected Picoscope. 
 * 
 * @param channel The channel to be shut off. 
 *                The following values are valid: 
 *                  - 0: Channel A
 *                  - 1: Channel B 
 *                  - 2: Channel C
 *                  - 3: Channel D
 *  
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
*/
int16_t set_channel_off(int channel) {

    status = ps6000aSetChannelOff(handle, channel);
    if (status != PICO_OK)
    {
        log_error("ps6000aSetChannelOff", status, __FILE__, __LINE__);
        return -1;
    }

    if (channel == CHANNEL_A) {
        channel_status.channel_a = 0;
    }
    if (channel == CHANNEL_B) {
        channel_status.channel_b = 0;
    }    
    if (channel == CHANNEL_C) {
        channel_status.channel_c = 0;
    }    
    if (channel == CHANNEL_D) {
        channel_status.channel_d = 0;
    }

    printf("Set channel %d off.\n", channel);
    return 0;
}

/**
 * Uses a requested sample interval to determine the closest timebase and sample interval that can 
 * be applied to the connected Picoscope given the resolution and number of channels enabled. 
 * 
 * @param requested_time_interval The requested sample interval in seconds. 
 *        timebase On exit, the value of the closest timebase for the requested interval. 
 *        available_time_interval On exit, the closests sample interval available, given the device configurations, 
 *                                to the request interval. 
 * 
 * @return 0 if the device is successfully opened, or -1 if an error occurs. 
 */
int16_t set_sample_interval(double requested_time_interval, uint32_t* timebase, double* available_time_interval){

    uint32_t timebase_return; 
    double time_interval_available;

    uint32_t enabledChannels = *(uint32_t*)&channel_status;

    status = ps6000aNearestSampleIntervalStateless(handle, enabledChannels, requested_time_interval, device_resolution, &timebase_return, &time_interval_available); 
    if (status != PICO_OK)
    {
        log_error("ps6000aNearestSampleIntervalStateless", status, __FILE__, __LINE__);
        return -1;
    }

    *timebase = timebase_return;
    *available_time_interval = time_interval_available; 

    return 0; 
} 

int16_t run_block_capture(struct SampleConfigs* sample_config, double* time_indisposed_ms);
int16_t set_trigger(struct ChannelConfigs* channel_config);
int16_t wait_for_capture_completion();
int16_t set_data_buffer(int16_t* waveform_buffer, struct ChannelConfigs* channel_config, struct SampleConfigs* sample_config);
int16_t retrieve_waveform_data(int16_t* waveform_buffer, struct SampleConfigs* sample_config);

/**
 * Retrieves waveform data from the Picoscope device based on the provided channel and sample configurations.
 * 
 * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @param waveform_buffer Pointer to the buffer where the retrieved waveform data will be stored.
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
int16_t retrieve_waveform(struct ChannelConfigs* channel_config, 
                          struct SampleConfigs* sample_config, 
                          int16_t* waveform_buffer) {
    int16_t status = 0;
    double time_indisposed_ms = 0;

    status = run_block_capture(sample_config, &time_indisposed_ms);
    if (status != PICO_OK) {
        return status;
    }
    status = wait_for_capture_completion();
    if (status != PICO_OK) {
        return status;
    }

    status = set_data_buffer(waveform_buffer, channel_config, sample_config);
    if (status != PICO_OK) {
        return status;
    }

    // status = set_trigger(channel_config);
    // if (status != PICO_OK) {
    //     return status;
    // }
    status = retrieve_waveform_data(waveform_buffer, sample_config);
    if (status != PICO_OK) {
        return status;
    }

    return status;
}

/**
 * Initiates a block capture on the Picoscope device.
 * 
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @param timebase The timebase value to use for the capture.
 * @param time_indisposed_ms Pointer to a variable where the time indisposed (in milliseconds) will be stored.
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
int16_t run_block_capture(struct SampleConfigs* sample_config, double* time_indisposed_ms) {
    uint64_t pre_trigger_samples = (uint64_t)sample_config->num_samples * sample_config->trigger_position_ratio;
    uint64_t post_trigger_samples = sample_config->num_samples - pre_trigger_samples;
    int16_t status = ps6000aRunBlock(
        handle,
        pre_trigger_samples,    
        post_trigger_samples,
        sample_config->timebase,
        time_indisposed_ms,
        0,
        NULL, 
        NULL
    );
    if (status != PICO_OK) {
        log_error("ps6000aRunBlock", status, __FILE__, __LINE__);
    }

    return status;
}

int16_t set_trigger(struct ChannelConfigs* channel_config) {
    ps6000aSetSimpleTrigger(
        handle,
        1,
        channel_config->channel,
        4064,
        PICO_FALLING,
        0,
        0
    );

    // ps6000aSetTriggerChannelConditions
    if (status != PICO_OK) {
        log_error("ps6000aRunBlock", status, __FILE__, __LINE__);
    }

    return status;
}
/**
 * Waits for the block capture to complete by polling the Picoscope device.
 * 
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
int16_t wait_for_capture_completion() {
    int16_t status = 0;
    int16_t trigger_status = 0;

    while (1) {
        status = ps6000aIsReady(handle, &trigger_status);
        if (status != PICO_OK) {
            log_error("ps6000aIsReady", status, __FILE__, __LINE__);
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

/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * 
 * @param waveform_buffer Pointer to the buffer where the waveform data will be stored.
 * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
int16_t set_data_buffer(int16_t* waveform_buffer, struct ChannelConfigs* channel_config, struct SampleConfigs* sample_config) {
    int16_t status = ps6000aSetDataBuffer(
        handle, channel_config->channel, NULL, 0, PICO_INT16_T, 0, 0, 
        PICO_CLEAR_ALL      // Clear buffer in Picoscope buffer list
    );
    if (status != PICO_OK) {
        log_error("ps6000aSetDataBuffer PICO_CLEAR_ALL", status, __FILE__, __LINE__);
    }

    status = ps6000aSetDataBuffer(
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
        log_error("ps6000aSetDataBuffer PICO_ADD", status, __FILE__, __LINE__);
    }

    return status;
}

/**
 * Retrieves the captured waveform data from the Picoscope device and stores it in the provided buffer.
 * 
 * @param waveform_buffer Pointer to the buffer where the waveform data will be stored.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
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
        log_error("ps6000aGetValues", status, __FILE__, __LINE__);
    }

    return status;
}