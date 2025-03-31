#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "ps6000aApi.h"
#include "PicoStatus.h"
#include <sys/time.h>
#include <epicsExport.h>
#include <iocsh.h>

#include <pthread.h>
#include "drvPicoscope.h"
#define MAX_PICO 10
static PS6000AModule *PS6000AModuleList[MAX_PICO] = {NULL};
pthread_mutex_t block_ready_mutex;
pthread_mutex_t ps6000a_call_mutex;
void log_error(char* function_name, uint32_t status, const char* FILE, int LINE){ 
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
 *        serial_num The serial number of the device to be opened. 
 *        handle On exit, the device identifier for future communication. 
 * 
 * @return 0 if the device is successfully opened, or a non-zero error code.  
*/
uint32_t open_picoscope(int16_t resolution, char* serial_num, int16_t* handle){

    int16_t handle_buffer; 
    uint32_t status = ps6000aOpenUnit(&handle_buffer, (int8_t*) serial_num, resolution);
    if (status != PICO_OK) 
    { 
        log_error("ps6000aOpenUnit", status, __FILE__, __LINE__); 
        return status;  
    }
    pthread_mutex_init(&block_ready_mutex, NULL);
    pthread_mutex_init(&ps6000a_call_mutex, NULL);

    *handle = handle_buffer;
    return 0;
}

/**
 * Close an open PicoScope device.
 *
 * @param handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if the device is successfully closed, or a non-zero error code. 
*/
uint32_t close_picoscope(int16_t handle){ 
    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aCloseUnit(handle);
    pthread_mutex_unlock(&ps6000a_call_mutex);
    if (status != PICO_OK) 
    { 
        log_error("ps6000aCloseUnit", status, __FILE__, __LINE__);
        return status;  
    } 
    pthread_mutex_destroy(&block_ready_mutex);
    pthread_mutex_destroy(&ps6000a_call_mutex);

    return 0;
}

/**
 * Check that open device is still connected. 
 * 
 * @param handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if device is connect, otherwise a non-zero error code.
*/
uint32_t ping_picoscope(int16_t handle){ 
    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aPingUnit(handle);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    // If driver call in progress, return connected. 
    if (status == PICO_DRIVER_FUNCTION) {
        return 0;    
    }
    if (status != PICO_OK) {
        log_error("ps6000aPingUnit. Cannot ping device.", status, __FILE__, __LINE__);
        return status; 
    }
    return 0;
}

/**
 * Set the sample resolution of the currently connected PicoScope. 
 * 
 * @param resolution The sampling resolution to be used.  
 *                   The following values are valid: 
 *                      - 0: 8-bit resolution
 *                      - 10: 10-bit resolution
 *                      - 1: 12-bit resolution
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if resolution successfully set, otherwise a non-zero error code.
*/
uint32_t set_device_resolution(int16_t resolution, int16_t handle){ 
   
    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aSetDeviceResolution(handle, resolution); 
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK){ 
        log_error("ps6000aSetDeviceResolution", status, __FILE__, __LINE__);
        return status;
    }

    return 0; 
}

/**
 * Get  the sample resolution of the currently connected PicoScope. 
 * 
 * @param on exit, the resolution of the device
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if resolution returned, otherwise a non-zero error code.
*/
uint32_t get_resolution(int16_t* resolution, int16_t handle) {

    PICO_DEVICE_RESOLUTION device_resolution; 
    
    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aGetDeviceResolution(handle, &device_resolution); 
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if(status != PICO_OK) {
        log_error("ps6000aGetDeviceResolution", status, __FILE__, __LINE__);
        return status;
    }

    *resolution = (int16_t)device_resolution; 
    return 0; 
}

/**
 * Retrieves the serial number of the currently connected PicoScope. 
 * 
 * @param serial_num A pointer to a pointer that will hold the dynamically allocated address of
 *                  the memory buffer containing the serial number. 
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if the serial number is successfully retrieved, otherwise a non-zero error code.
*/
uint32_t get_serial_num(int8_t** serial_num, int16_t handle) {

    int16_t required_size = 0; 

    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_BATCH_AND_SERIAL);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    int8_t* serial_num_buffer = calloc(required_size, sizeof(int8_t)); 

    pthread_mutex_lock(&ps6000a_call_mutex);
    status = ps6000aGetUnitInfo(handle, serial_num_buffer, required_size, &required_size, PICO_BATCH_AND_SERIAL);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    *serial_num = serial_num_buffer;


    return 0;  
}


/**
 * Retrieves the model number of the currently connected PicoScope. 
 * 
 * @param model_num A pointer to a pointer that will hold the dynamically allocated address of
 *                  the memory buffer containing the model number. 
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if the model number is successfully retrieved, otherwise a non-zero error code.
*/
uint32_t get_model_num(int8_t** model_num, int16_t handle) {

    int16_t required_size = 0; 

    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_VARIANT_INFO);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    int8_t* model_num_buffer = calloc(required_size, sizeof(int8_t));

    
    pthread_mutex_lock(&ps6000a_call_mutex);
    status = ps6000aGetUnitInfo(handle, model_num_buffer, required_size, &required_size, PICO_VARIANT_INFO);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    *model_num = model_num_buffer;
    
    return 0;
}


/**
 * Retrieves the model and serial number of the currently connected PicoScope. 
 * 
 * @param device_info A pointer to a pointer that will hold the dynamically allocated address of
 *                    the memory buffer containing the device information. 
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return 0 if the device information is successfully retrieved, otherwise a non-zero error code.
*/
uint32_t get_device_info(int8_t** device_info, int16_t handle) {

    int8_t* serial_num = NULL;
    int8_t* model_num = NULL; 

    uint32_t status = get_serial_num(&serial_num, handle);
    if (status == 0) {
        status = get_model_num(&model_num, handle); 
        if (status == 0) {
            const static char* FORMAT_STR = "Picoscope %s [%s]";
            int16_t required_size = strlen((const char*)serial_num) + strlen((const char*)model_num) + *FORMAT_STR; 

            int8_t* device_info_buffer = malloc(required_size);
            snprintf((char*)device_info_buffer, required_size, FORMAT_STR, (char*)model_num, (char*)serial_num);
            *device_info = device_info_buffer;
        }
    }

    free(serial_num); 
    free(model_num);

    return status; 
}


/**
 * Enables a specified channel on the connected Picocope with the given configurations. 
 * Setting the channels coupling, range, analog offset, and bandwidth. 
 * 
 * @param channel `ChannelConfigs` structure that contains the configuration to be activated. 
 *                The structure holds the coupling type, voltage range, analog offset, and 
 *                bandwidth to configure the channel. 
 *        handle The device identifier returned by open_picoscope(). 
 *        channel_status On exit, the updated status of Picoscope channels.
 * 
 * @return 0 if the channel is succesfully set on, otherwise a non-zero error code. 
*/
uint32_t set_channel_on(struct ChannelConfigs channel, int16_t handle, EnabledChannelFlags* channel_status) {

    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aSetChannelOn(handle, channel.channel, channel.coupling, channel.range, channel.analog_offset, channel.bandwidth);
    pthread_mutex_unlock(&ps6000a_call_mutex);
    if (status != PICO_OK) 
    {
        log_error("ps6000aSetChannelOn", status, __FILE__, __LINE__);
        return status;
    }

    switch (channel.channel)
    {
        case CHANNEL_A:
            channel_status->channel_a = 1;
            break;
        case CHANNEL_B:
            channel_status->channel_b = 1;       
            break;
        case CHANNEL_C:
            channel_status->channel_c = 1;
            break;
        case CHANNEL_D:
            channel_status->channel_d = 1;
            break;
        default:
            return -1;
    }

    printf("Setting channel %d on.\n", channel.channel);
  
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
 *        handle The device identifier returned by open_picoscope(). 
 *        channel_status On exit, the updated status of Picoscope channels.
 *  
 * @return 0 if the channel is successfully turned off, otherwise a non-zero error code.
*/
uint32_t set_channel_off(int channel, int16_t handle, EnabledChannelFlags* channel_status) {
    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aSetChannelOff(handle, channel);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK)
    {
        log_error("ps6000aSetChannelOff", status, __FILE__, __LINE__);
        return status;
    }

    switch (channel)
    {
        case CHANNEL_A:
            channel_status->channel_a = 0;
            break;
        case CHANNEL_B:
            channel_status->channel_b = 0;       
            break;
        case CHANNEL_C:
            channel_status->channel_c = 0;
            break;
        case CHANNEL_D:
            channel_status->channel_d = 0;
            break;
        default:
            return -1;
    }

    printf("Set channel %d off.\n", channel);
    return 0;
}

/**
 * Check if the status of the specified channel. 
 * 
 * @param channel The channel you wish to check the status of. 
 *                The following values are valid: 
 *                  - 0: Channel A
 *                  - 1: Channel B 
 *                  - 2: Channel C
 *                  - 3: Channel D 
 * 
 * @return 1 if the channel is enabled, 0 if not, and -1 if channel does not exist
 */
uint32_t get_channel_status(int16_t channel, EnabledChannelFlags channel_status){ 
    
    switch (channel)
    {
        case CHANNEL_A:
            return channel_status.channel_a;
        case CHANNEL_B:
            return channel_status.channel_b;
        case CHANNEL_C:
            return channel_status.channel_c;
        case CHANNEL_D:
            return channel_status.channel_d;
        default:
            return -1;
    }
}   

/**
 * Uses the range and coupling of a specific channel to retrieve the maximum and minimum analog offset voltages possible. 
 * 
 * @param `ChannelConfigs` structure that contains the configurations of the channel in question. 
 *        handle The device identifier returned by open_picoscope(). 
 *        max_analog_offset On exit, the max analog offset voltage allowed for the range. 
 *        min_analog_offset On exit, the min analog offset voltage allowed for the range. 
 * 
 * @return 0 if the analog offset limits are succesfully retrieved, otherwise a non-zero error code. 
 */
uint32_t get_analog_offset_limits(struct ChannelConfigs channel, int16_t handle, double* max_analog_offset, double* min_analog_offset){

    double maximum_voltage; 
    double minimum_voltage;

    pthread_mutex_lock(&ps6000a_call_mutex);
    uint32_t status = ps6000aGetAnalogueOffsetLimits(handle, channel.range, channel.coupling, &maximum_voltage, &minimum_voltage); 
    pthread_mutex_unlock(&ps6000a_call_mutex);
    if (status != PICO_OK)
    {
        log_error("ps6000aGetAnalogueOffsetLimits", status, __FILE__, __LINE__);
        return status;
    }

    *max_analog_offset = maximum_voltage;
    *min_analog_offset = minimum_voltage;

    return 0; 
}

/**
 * Uses a requested sample interval to determine the closest timebase and sample interval that can 
 * be applied to the connected Picoscope given the resolution and number of channels enabled. 
 * 
 * @param requested_time_interval The requested sample interval in seconds. 
 *        handle The device identifier returned by open_picoscope(). 
 *        channel_status EnabledChannelFlags of channels currently enabled on the device identified by the handle. 
 *        timebase On exit, the value of the closest timebase for the requested interval. 
 *        available_time_interval On exit, the closests sample interval available, given the device configurations, 
 *                                to the request interval. 
 * 
 * @return 0 if the call is successful, otherwise a non-zero error code.
 */
uint32_t validate_sample_interval(double requested_time_interval, int16_t handle, EnabledChannelFlags channel_status, uint32_t* timebase, double* available_time_interval){

    uint32_t enabledChannels = *(uint32_t*)&channel_status;

    int16_t resolution = 0; 
    uint32_t status = get_resolution(&resolution, handle);
    if (status != PICO_OK) {
        return status;
    }

    uint32_t timebase_return; 
    double time_interval_available;

    pthread_mutex_lock(&ps6000a_call_mutex);
    status = ps6000aNearestSampleIntervalStateless(handle, enabledChannels, requested_time_interval, resolution, &timebase_return, &time_interval_available); 
    pthread_mutex_unlock(&ps6000a_call_mutex);
    
    if (status == PICO_NO_CHANNELS_OR_PORTS_ENABLED) {
        log_error("ps6000aNearestSampleIntervalStateless. No channels enabled.", status, __FILE__, __LINE__);
        return status;
    }
    if (status != PICO_OK)
    {
        log_error("ps6000aNearestSampleIntervalStateless", status, __FILE__, __LINE__);
        return status;
    }

    *timebase = timebase_return;
    *available_time_interval = time_interval_available; 


    return 0; 
} 


/** 
 * Converts a time in some unit to seconds. 
 * 
 * @params time An amount of time. 
 *         unit The unit used for time.  
 * 
 * @returns The time converted to seconds, or -1 if conversion failed. 
*/
double convert_to_seconds(double time, enum UnitPerDiv unit) {
    switch (unit)
    {
        case ns_per_div:
            return time / 1000000000; 

        case us_per_div:
            return time / 1000000; 

        case ms_per_div:
            
            return time / 1000; 

        case s_per_div:
            return time; 

        default:
            return -1; 
    }
}

double calculate_samples_per_division(uint64_t num_samples, int16_t num_division) {
    return (double) num_samples / num_division;       
}

double calculate_sample_interval(double secs_per_div, double samples_per_div){ 
    return secs_per_div / samples_per_div;    
}

double calculate_sample_rate(double secs_per_div, double samples_per_div) {
    return samples_per_div / secs_per_div; 
}

/**
 *  Gets the valid timebase configs given the requested time per division, number of divisions, and number of samples. 
 * 
 * @param mp Pointer to the PS6000AModule structure containing Picoscope device information. 
 *        sample_interval On exit, the interval at which samples will be taken in seconds. 
 *        timebase On exit, the timebase for the requested time per division. 
 *        sample_rate On exit, the sample rate for the request time per division. 
 * 
 * @return 0 if successful, otherwise a non-zero error code.
 */
uint32_t get_valid_timebase_configs(struct PS6000AModule* mp, double* sample_interval, uint32_t* timebase, double* sample_rate) { 

    double secs_per_div = convert_to_seconds(mp->sample_config.timebase_configs.time_per_division, mp->sample_config.timebase_configs.time_per_division_unit); 
    double samples_per_division = calculate_samples_per_division(mp->sample_config.num_samples, mp->sample_config.timebase_configs.num_divisions);

    double requested_sample_interval = calculate_sample_interval(secs_per_div, samples_per_division); 

    *sample_rate = calculate_sample_rate(secs_per_div, samples_per_division); 

    uint32_t available_timebase; 
    double available_sample_interval; 

    uint32_t status = validate_sample_interval(requested_sample_interval, mp->handle, mp->channel_status, &available_timebase, &available_sample_interval);
    if (status != 0) {
        return status; 
    } 
    *sample_interval = available_sample_interval; 
    *timebase = available_timebase;

    return 0;
}


typedef struct {
    PICO_STATUS callbackStatus; // Status from the callback
    int dataReady;
    struct PS6000AModule* mp;

} BlockReadyCallbackParams;
PICO_STATUS setup_picoscope(struct PS6000AModule* mp, int16_t* waveform_buffer[CHANNEL_NUM]);
PICO_STATUS init_block_ready_callback_params(struct PS6000AModule* mp);
PICO_STATUS run_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms);
PICO_STATUS set_data_buffer(int16_t* waveform_buffer[CHANNEL_NUM], struct ChannelConfigs channel_config[CHANNEL_NUM], struct SampleConfigs sample_config, EnabledChannelFlags channel_status, int16_t handle);
PICO_STATUS set_trigger_configurations(struct TriggerConfigs trigger_config, int16_t handle);
PICO_STATUS start_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms);
PICO_STATUS wait_for_capture_completion(struct PS6000AModule* mp);
PICO_STATUS retrieve_waveform_data(struct PS6000AModule* mp);

BlockReadyCallbackParams* blockReadyCallbackParams;
/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * 
 * @param waveform_buffer Pointer to the buffer where the waveform data will be stored.
 * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS setup_picoscope(struct PS6000AModule* mp, int16_t* waveform_buffer[CHANNEL_NUM]) {

    PICO_STATUS status = 0;
    init_block_ready_callback_params(mp);
    if (status != PICO_OK) {
        return status;
    }
    if (mp->trigger_config.triggerType == NO_TRIGGER) {
        // If no trigger set, clear previous triggers and do not set new one 
        PICO_CONDITION condition;
        status = ps6000aSetTriggerChannelConditions(mp->handle, &condition, 0, PICO_CLEAR_ALL);   
        if (status != PICO_OK) {
            return status;
        }        
        printf("No trigger set.\n");
    } 
    else { 
        status = set_trigger_configurations(mp->trigger_config, mp->handle);
        if (status != PICO_OK) {
            return status;
        }
        printf("Trigger set.\n");
    }

    status = set_data_buffer(waveform_buffer, mp->channel_configs, mp->sample_config, mp->channel_status, mp->handle);
    if (status != PICO_OK) {
        return status;
    }
    return status;
}



PICO_STATUS set_trigger_conditions(struct TriggerConfigs trigger_config, int16_t handle) {
    int16_t nConditions = 1;
    PICO_STATUS status = 0;

    PICO_CONDITION condition = {
        .source = trigger_config.channel,
        .condition = PICO_CONDITION_TRUE
    };
    pthread_mutex_lock(&ps6000a_call_mutex);
    ps6000aSetTriggerChannelConditions(handle, &condition, nConditions, PICO_CLEAR_ALL);
    ps6000aSetTriggerChannelConditions(handle, &condition, nConditions, PICO_ADD);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelConditions", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

PICO_STATUS set_trigger_directions(struct TriggerConfigs trigger_config, int16_t handle) {
    int16_t nDirections = 1;    // Only support one now 
    PICO_STATUS status = 0;
    if(trigger_config.thresholdDirection == 10){
        trigger_config.thresholdDirection = PICO_NEGATIVE_RUNT;
    }
    PICO_DIRECTION direction = {
        .channel = trigger_config.channel,
        .direction = trigger_config.thresholdDirection,
        .thresholdMode = trigger_config.thresholdMode
    };
    pthread_mutex_lock(&ps6000a_call_mutex);
    ps6000aSetTriggerChannelDirections(handle, &direction, nDirections);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelDirections", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

PICO_STATUS set_trigger_properties(struct TriggerConfigs trigger_config, int16_t handle) {
    int16_t nChannelProperties = 1; // Only support one now 
    unsigned short hysteresis = (unsigned short)((UINT16_MAX / 100.0) * 5.0);   // 5% of the full range

    PICO_TRIGGER_CHANNEL_PROPERTIES channelProperty = { 
        .channel = trigger_config.channel,
        .thresholdUpper = trigger_config.thresholdUpper,
        .thresholdUpperHysteresis = hysteresis,
        .thresholdLower = trigger_config.thresholdLower,
        .thresholdLowerHysteresis = hysteresis
    };
    pthread_mutex_lock(&ps6000a_call_mutex);
    PICO_STATUS status = ps6000aSetTriggerChannelProperties(handle, &channelProperty, nChannelProperties, 0, trigger_config.autoTriggerMicroSeconds);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelProperties", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * 
 * @param waveform_buffer Pointer to the buffer where the waveform data will be stored.
 * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS set_data_buffer(int16_t* waveform_buffer[CHANNEL_NUM], struct ChannelConfigs channel_config[CHANNEL_NUM], struct SampleConfigs sample_config, EnabledChannelFlags channel_status, int16_t handle) {
    pthread_mutex_lock(&ps6000a_call_mutex);
    PICO_STATUS status = ps6000aSetDataBuffer(
        handle, (PICO_CHANNEL)NULL, NULL, 0, PICO_INT16_T, 0, 0, 
        PICO_CLEAR_ALL      // Clear buffer in Picoscope buffer list
    );
    pthread_mutex_unlock(&ps6000a_call_mutex);
    
    for (size_t i = 0; i < CHANNEL_NUM; i++)
    {
        if (status != PICO_OK) {
            log_error("ps6000aSetDataBuffer PICO_CLEAR_ALL", status, __FILE__, __LINE__);
        }
        if (get_channel_status(channel_config[i].channel, channel_status))
        {
            pthread_mutex_lock(&ps6000a_call_mutex);
            status = ps6000aSetDataBuffer(
                handle, 
                channel_config[i].channel, 
                waveform_buffer[i], 
                sample_config.num_samples, 
                PICO_INT16_T, 
                0, 
                sample_config.down_sample_ratio_mode, 
                PICO_ADD
            );
            pthread_mutex_unlock(&ps6000a_call_mutex);
            if (status != PICO_OK) {
                log_error("ps6000aSetDataBuffer PICO_ADD", status, __FILE__, __LINE__);
            }
        }
    }
    

    return status;
}

PICO_STATUS set_trigger_configurations(struct TriggerConfigs trigger_config, int16_t handle) {
    
    PICO_STATUS status = set_trigger_conditions(trigger_config, handle);
    if (status != PICO_OK) return status;

    status = set_trigger_directions(trigger_config, handle);
    if (status != PICO_OK) return status;

    status = set_trigger_properties(trigger_config, handle);
    if (status != PICO_OK) return status;

    return status;
}

PICO_STATUS init_block_ready_callback_params(struct PS6000AModule* mp){
    free(blockReadyCallbackParams);
    blockReadyCallbackParams = (BlockReadyCallbackParams*)malloc(sizeof(BlockReadyCallbackParams));
    if (blockReadyCallbackParams == NULL) {
        log_error("BlockReadyCallbackParams malloc", PICO_MEMORY_FAIL, __FILE__, __LINE__);
        return PICO_MEMORY_FAIL;
    }
    memset(blockReadyCallbackParams, 0, sizeof(BlockReadyCallbackParams));
    blockReadyCallbackParams->mp = mp;
    return PICO_OK;
}

/**
 * Initiates a block capture on the Picoscope device.
 * 
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @param time_indisposed_ms Pointer to a variable where the time indisposed (in milliseconds) will be stored.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS run_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms) {
    PICO_STATUS status = 0;

    status = start_block_capture(mp, time_indisposed_ms);
    if (status != PICO_OK) {
        log_error("start_block_capture", status, __FILE__, __LINE__);
        return status;
    }

    status = wait_for_capture_completion(mp);
    if (status != PICO_OK && status != PICO_CANCELLED) {
        log_error("wait_for_capture_completion", status, __FILE__, __LINE__);
        return status;
    }

    return PICO_OK;
}


void ps6000aBlockReadyCallback(int16_t handle, PICO_STATUS status, void *pParameter)
{
    BlockReadyCallbackParams *state = (BlockReadyCallbackParams *)pParameter;
    state->callbackStatus = status;
    struct timeval tv1;
    struct tm *tm_info1;
    gettimeofday(&tv1, NULL); 
    tm_info1 = localtime(&tv1.tv_sec);
    printf("ps6000aBlockReadyCallback Trigger Captured : %04d-%02d-%02d %02d:%02d:%02d.%06ld\n",
         tm_info1->tm_year + 1900,
         tm_info1->tm_mon + 1,    
         tm_info1->tm_mday,       
         tm_info1->tm_hour,       
         tm_info1->tm_min,        
         tm_info1->tm_sec,        
         tv1.tv_usec); 
    if (status == PICO_CANCELLED)
    {
        state->dataReady = 0;
        printf("Data capturing cancelled\n");
    }else if (status == PICO_OK)
    {
        state->dataReady = 1;
        epicsEventSignal(state->mp->triggerReadyEvent);
    }else{
        log_error("ps6000aBlockReadyCallback", status, __FILE__, __LINE__);
    }
}

/**
 * Initiates a block capture on the Picoscope device.
 * 
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @param timebase The timebase value to use for the capture.
 * @param time_indisposed_ms Pointer to a variable where the time indisposed (in milliseconds) will be stored.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS start_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms) {
    PICO_STATUS ps6000aRunBlockStatus;
    PICO_STATUS ps6000aStopStatus;
    struct SampleConfigs sample_config = mp->sample_config;
    uint64_t pre_trigger_samples = ((uint64_t)sample_config.num_samples * sample_config.trigger_position_ratio)/100;
    uint64_t post_trigger_samples = sample_config.num_samples - pre_trigger_samples;

    blockReadyCallbackParams->dataReady = 0;
    int8_t runBlockCaptureRetryFlag = 0;

    pthread_mutex_lock(&ps6000a_call_mutex);
    do {
        ps6000aRunBlockStatus = ps6000aRunBlock(
            mp->handle,
            pre_trigger_samples,    
            post_trigger_samples,
            sample_config.timebase_configs.timebase,
            time_indisposed_ms,
            0,
            ps6000aBlockReadyCallback, 
            (void*) blockReadyCallbackParams
        );

        if (ps6000aRunBlockStatus == PICO_HARDWARE_CAPTURING_CALL_STOP) {
            runBlockCaptureRetryFlag ++;
            printf("ps6000aRunBlock Retry attempt: %d\n", runBlockCaptureRetryFlag);
            ps6000aStopStatus = ps6000aStop(mp->handle);
            if (ps6000aStopStatus != PICO_OK) {
                printf("Error: Failed to stop capture in ps6000aRunBlock, status: %d\n", ps6000aStopStatus);
                return ps6000aStopStatus;
            }
        }
    } while (ps6000aRunBlockStatus == PICO_HARDWARE_CAPTURING_CALL_STOP);
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL); 
    tm_info = localtime(&tv.tv_sec);
    printf("ps6000aRunBlock           Trigger Listening: %04d-%02d-%02d %02d:%02d:%02d.%06ld\n",
         tm_info->tm_year + 1900,
         tm_info->tm_mon + 1,    
         tm_info->tm_mday,       
         tm_info->tm_hour,       
         tm_info->tm_min,        
         tm_info->tm_sec,        
         tv.tv_usec); 
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (ps6000aRunBlockStatus != PICO_OK) {
        log_error("ps6000aRunBlock", ps6000aRunBlockStatus, __FILE__, __LINE__);
        return ps6000aRunBlockStatus;
    }

    return PICO_OK;
}

/**
 * Waits for the block capture to complete by polling the Picoscope device.
 * 
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS wait_for_capture_completion(struct PS6000AModule* mp)
{
    PICO_STATUS status = PICO_OK;
    while (1)
    {
        int returnStatus = epicsEventWait((epicsEventId)mp->triggerReadyEvent);
        if (returnStatus == epicsEventWaitOK){
            break;
        }
    }

    if (!blockReadyCallbackParams->dataReady) {
        mp->sample_collected = 0;
        return PICO_CANCELLED;
    }

   // printf("Capture finished.\n");
    status = retrieve_waveform_data(mp);

    if (status != PICO_OK) {
        return status;
    }  

    return PICO_OK;
}

/**
 * Retrieves the captured waveform data from the Picoscope device and stores it in the provided buffer.
 * 
 * @param mp Pointer to the PS6000AModule structure containing sample-collection settings.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS retrieve_waveform_data(struct PS6000AModule* mp) {
    uint64_t start_index = 0;
    uint64_t segment_index = 0;
    int16_t overflow = 0;
    int8_t getValueRetryFlag = 0;
    PICO_STATUS ps6000aStopStatus;
    PICO_STATUS ps6000aGetValuesStatus;

    pthread_mutex_lock(&ps6000a_call_mutex);
    do {
        ps6000aGetValuesStatus = ps6000aGetValues(
            mp->handle,
            start_index,
            &mp->sample_collected,
            mp->sample_config.down_sample_ratio,
            mp->sample_config.down_sample_ratio_mode,
            segment_index,
            &overflow
        );

        if (ps6000aGetValuesStatus == PICO_HARDWARE_CAPTURING_CALL_STOP) {
            getValueRetryFlag++;
            printf("dataReady %d\n",blockReadyCallbackParams->dataReady);
            printf("ps6000aGetValues Retry attempt: %d\n", getValueRetryFlag);
            ps6000aStopStatus = ps6000aStop(mp->handle);
            if (ps6000aStopStatus != PICO_OK) {
                printf("Error: Failed to stop capture, status: %d\n", ps6000aStopStatus);
                return ps6000aStopStatus;
            }
        }
    } while (ps6000aGetValuesStatus == PICO_HARDWARE_CAPTURING_CALL_STOP);
    pthread_mutex_unlock(&ps6000a_call_mutex);

    if (ps6000aGetValuesStatus != PICO_OK) {
        log_error("ps6000aGetValues", ps6000aGetValuesStatus, __FILE__, __LINE__);
        return ps6000aGetValuesStatus;
    }


    return PICO_OK;
}

PICO_STATUS stop_capturing(int16_t handle) {
    PICO_STATUS status;
    printf("Capture stopping.\n");  
    pthread_mutex_lock(&ps6000a_call_mutex);
    status = ps6000aStop(handle);
    pthread_mutex_unlock(&ps6000a_call_mutex);
    printf("Capture stopped.\n");     
    if (status != PICO_OK) {
        log_error("wait_for_capture_completion ps6000aStop", status, __FILE__, __LINE__);
    }
    return status;
}

struct PS6000AModule*
PS6000AGetModule(char* serial_num){
    for (size_t i = 0; i < MAX_PICO; i++) {
        if (PS6000AModuleList[i] != NULL && strcmp(serial_num, PS6000AModuleList[i]->serial_num) == 0) {
            return PS6000AModuleList[i];
        }
    }
    log_error("PS6000AGetModule. Device does not exist.", PICO_NOT_FOUND, __FILE__, __LINE__);
    return NULL;
}

struct PS6000AModule*
PS6000ACreateModule(char* serial_num){
    struct PS6000AModule* mp = calloc(1, sizeof(struct PS6000AModule));
    if (!mp) {
        log_error("PS6000ACreateModule calloc", PICO_MEMORY_FAIL, __FILE__, __LINE__);
        return NULL;
    }
    mp->serial_num = strdup(serial_num);
    if (!mp->serial_num) {
        free(mp);
        log_error("PS6000ACreateModule strdup", PICO_MEMORY_FAIL, __FILE__, __LINE__);
        return NULL;
    }
    mp->triggerReadyEvent = epicsEventCreate(0);

    for (size_t i = 0; i < MAX_PICO; i++) {
        if (PS6000AModuleList[i] == NULL) {
            PS6000AModuleList[i] = mp;
            return mp;
        }
    }
    free(mp->serial_num);
    free(mp);
    log_error("PS6000ACreateModule", PICO_NO_APPS_AVAILABLE, __FILE__, __LINE__);
    return NULL;
}

static int
PS6000ASetup(char* serial_num){
    printf("PS6000ASetup(%s)\n", serial_num);
    struct PS6000AModule* mp = PS6000AGetModule(serial_num);
    if (!mp) {
        printf("Module does not exist, lets go make one\n");
        if ((mp = PS6000ACreateModule(serial_num)) == NULL) {
            return PICO_MEMORY_FAIL;
        }
    }
    return 0;
}

static void
PS6000ASetupCB( const iocshArgBuf *arglist)
{
	PS6000ASetup(arglist[0].sval);
}


static iocshArg setPS6000AArg0 = { "Serial Number", iocshArgString };
// static iocshArg setPS6000AArg1 = { "module", iocshArgInt };
// static iocshArg setPS6000AArg2 = { "level", iocshArgInt };
// static iocshArg setPS6000AArg3 = { "vector", iocshArgInt };
// static iocshArg setPS6000AArg4 = { "waveformLength", iocshArgInt };
// static iocshArg * setPS6000AArgs[5] = { &setPS6000AArg0, &setPS6000AArg1, &setPS6000AArg2, &setPS6000AArg3, &setPS6000AArg4 };
static const iocshArg * setPS6000AArgs[1] = {&setPS6000AArg0};
static iocshFuncDef PS6000ASetupDef = {"PS6000ASetup", 1, &setPS6000AArgs[0]};

void registerPS6000A(void)
{
	iocshRegister( &PS6000ASetupDef, PS6000ASetupCB);
}

epicsExportRegistrar(registerPS6000A);
