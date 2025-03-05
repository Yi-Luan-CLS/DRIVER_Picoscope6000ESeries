#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
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
 * @return 0 if the device is successfully closed, or -1 if an error occurs. 
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
    if (status != PICO_OK && status != PICO_DRIVER_FUNCTION) {
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
int16_t get_resolution(int16_t* resolution) {

    PICO_DEVICE_RESOLUTION device_resolution; 

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
 * @return 0 if the serial number is successfully retrieved, or -1 if an error occurs. 
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
 * @return 0 if the model number is successfully retrieved, or -1 if an error occurs. 
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
 * @return 0 if the device information is successfully retrieved, or -1 if an error occurs. 
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
 * Setting the channels coupling, range, analog offset, and bandwidth. 
 * 
 * @param channel A pointer to a `ChannelConfigs` structure that contains the configuration 
 *                to be activated. The structure holds the coupling type, voltage range, analog
 *                offset, and bandwidth to configure the channel. 
 * 
 * @return 0 if the channel is succesfully set on, or -1 if an error occurs. 
*/
int16_t set_channel_on(struct ChannelConfigs* channel) {
    
    status = ps6000aSetChannelOn(handle, channel->channel, channel->coupling, channel->range, channel->analog_offset, channel->bandwidth);
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
 * @return 0 if the channel is successfully turned off, or -1 if an error occurs. 
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
int16_t get_channel_status(int16_t channel){ 
    
    if (channel == CHANNEL_A) {
        return channel_status.channel_a;
    }
    if (channel == CHANNEL_B) {
        return channel_status.channel_b;
    }    
    if (channel == CHANNEL_C) {
        return channel_status.channel_c;
    }    
    if (channel == CHANNEL_D) {
        return channel_status.channel_d;
    }
    return -1; 

}

/**
 * Uses the range and coupling of a specific channel to retrieve the maximum and minimum analog offset voltages possible. 
 * 
 * @param range The voltage range set to a channel. See PICO_CONNECT_PROBE_RANGE in PicoConnectProbes.h. 
 *        coupling The coupling set to a channel. See PICO_COUPLING in PicoDeviceEnums.h.
 *        max_analog_offset On exit, the max analog offset voltage allowed for the range. 
 *        min_analog_offset On exit, the min analog offset voltage allowed for the range. 
 * 
 * @return 0 if the analog offset limits are succesfully retrieved, or -1 if an error occurs. 
 */
int16_t get_analog_offset_limits(int16_t range, int16_t coupling, double* max_analog_offset, double* min_analog_offset){

    double maximum_voltage; 
    double minimum_voltage;

    status = ps6000aGetAnalogueOffsetLimits(handle, range, coupling, &maximum_voltage, &minimum_voltage); 
    if (status != PICO_OK)
    {
        log_error("ps6000aGetAnalogueOffsetLimits", status, __FILE__, __LINE__);
        return -1;
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
 *        timebase On exit, the value of the closest timebase for the requested interval. 
 *        available_time_interval On exit, the closests sample interval available, given the device configurations, 
 *                                to the request interval. 
 * 
 * @return 0 if the call is successful, or -1 if an error occurs. 
 */
int16_t validate_sample_interval(double requested_time_interval, uint32_t* timebase, double* available_time_interval){

    uint32_t enabledChannels = *(uint32_t*)&channel_status;

    int16_t resolution = 0; 
    status = get_resolution(&resolution);

    uint32_t timebase_return; 
    double time_interval_available;

    status = ps6000aNearestSampleIntervalStateless(handle, enabledChannels, requested_time_interval, resolution, &timebase_return, &time_interval_available); 
    
    if (status == PICO_NO_CHANNELS_OR_PORTS_ENABLED) {
        log_error("ps6000aNearestSampleIntervalStateless. No channels enabled.", status, __FILE__, __LINE__);
        return -1;
    }
    if (status != PICO_OK)
    {
        log_error("ps6000aNearestSampleIntervalStateless", status, __FILE__, __LINE__);
        return -1;
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
 * @param timebase_configs TimebaseConfigs structure containing timebase settings. 
 *        num_samples The number of requested samples.  
 *        sample_interval On exit, the interval at which samples will be taken in seconds. 
 *        timebase On exit, the timebase for the requested time per division. 
 *        sample_rate On exit, the sample rate for the request time per division. 
 * 
 * @return 0 if successful, otherwise -1. 
 */
int16_t get_valid_timebase_configs(struct TimebaseConfigs timebase_configs, uint64_t num_samples, double* sample_interval, uint32_t* timebase, double* sample_rate) { 

    double secs_per_div = convert_to_seconds(timebase_configs.time_per_division, timebase_configs.time_per_division_unit); 
    double samples_per_division = calculate_samples_per_division(num_samples, timebase_configs.num_divisions);

    double requested_sample_interval = calculate_sample_interval(secs_per_div, samples_per_division); 

    *sample_rate = calculate_sample_rate(secs_per_div, samples_per_division); 

	uint32_t available_timebase; 
	double available_sample_interval; 

	int16_t result = validate_sample_interval(requested_sample_interval, &available_timebase, &available_sample_interval);
	if (result != 0) {
        return -1; 
	} 
    *sample_interval = available_sample_interval; 
    *timebase = available_timebase;

    return 0;
}


typedef struct {
    int dataReady; // Flag to indicate data is ready
    PICO_STATUS callbackStatus; // Status from the callback
} BlockCaptureState;
PICO_STATUS setup_picoscope(int16_t* waveform_buffer[CHANNEL_NUM], struct ChannelConfigs* channel_config[CHANNEL_NUM], struct SampleConfigs* sample_config, struct TriggerConfigs* trigger_config);
PICO_STATUS run_block_capture(struct SampleConfigs* sample_config, double* time_indisposed_ms, uint8_t* capturing);
PICO_STATUS set_data_buffer(int16_t* waveform_buffer[CHANNEL_NUM], struct ChannelConfigs* channel_config[CHANNEL_NUM], struct SampleConfigs* sample_config);
PICO_STATUS set_AUX_trigger(struct TriggerConfigs* trigger_config);
PICO_STATUS start_block_capture(struct SampleConfigs* sample_config, double* time_indisposed_ms);
PICO_STATUS wait_for_capture_completion(struct SampleConfigs* sample_config, uint8_t* capturing);
PICO_STATUS retrieve_waveform_data(struct SampleConfigs* sample_config);

// /**
//  * Retrieves waveform data from the Picoscope device based on the provided channel and sample configurations.
//  * 
//  * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
//  * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
//  * @param waveform_buffer Pointer to the buffer where the retrieved waveform data will be stored.
//  * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
//  */
// int16_t retrieve_waveform(struct ChannelConfigs* channel_config, 
//                           struct SampleConfigs* sample_config, 
//                           int16_t* waveform_buffer) {
//     int16_t status = 0;
//     double time_indisposed_ms = 0;
//     struct TriggerConfigs trigger_config = {
//         .channel = PICO_TRIGGER_AUX,
//         .thresholdMode = PICO_LEVEL,
//         .thresholdUpper = 0,
//         .thresholdLower = 0,
//         .thresholdDirection = PICO_RISING
//     };

//     status = setup_picoscope(waveform_buffer, channel_config, sample_config, &trigger_config);
//     if (status != PICO_OK) {
//         return status;
//     }

//     status = run_block_capture(sample_config, &time_indisposed_ms);
//     if (status != PICO_OK) {
//         return status;
//     }

//     return status;
// }
BlockCaptureState* callback_state;
/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * 
 * @param waveform_buffer Pointer to the buffer where the waveform data will be stored.
 * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return int16_t Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS setup_picoscope(int16_t* waveform_buffer[CHANNEL_NUM], struct ChannelConfigs* channel_config[CHANNEL_NUM], struct SampleConfigs* sample_config, struct TriggerConfigs* trigger_config) {

    PICO_STATUS status = 0;

    status = set_AUX_trigger(trigger_config);
    if (status != PICO_OK) {
        return status;
    }

    status = set_data_buffer(waveform_buffer, channel_config, sample_config);
    if (status != PICO_OK) {
        return status;
    }
    return status;
}

PICO_STATUS set_trigger_conditions(struct TriggerConfigs* trigger_config) {
    int16_t nConditions = 1;
    PICO_STATUS status = 0;

    PICO_CONDITION condition = {
        .source = trigger_config->channel,
        .condition = PICO_CONDITION_TRUE
    };
    ps6000aSetTriggerChannelConditions(handle, &condition, nConditions, PICO_CLEAR_ALL);
    ps6000aSetTriggerChannelConditions(handle, &condition, nConditions, PICO_ADD);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelConditions", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

PICO_STATUS set_trigger_directions(struct TriggerConfigs* trigger_config) {
    int16_t nDirections = 1;    // Only support one now 
    PICO_STATUS status = 0;
    PICO_DIRECTION direction = {
        .channel = trigger_config->channel,
        .direction = trigger_config->thresholdDirection,
        .thresholdMode = trigger_config->thresholdMode
    };
    ps6000aSetTriggerChannelDirections(handle, &direction, nDirections);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelDirections", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

PICO_STATUS set_trigger_properties(struct TriggerConfigs* trigger_config) {
    int16_t nChannelProperties = 1; // Only support one now 
    uint32_t autoTriggerMicroSeconds = 0;     // Keep waiting until triggered
    unsigned short hysteresis = (unsigned short)((UINT16_MAX / 100.0) * 5.0);   // 5% of the full range

    PICO_TRIGGER_CHANNEL_PROPERTIES channelProperty = { 
        .channel = trigger_config->channel,
        .thresholdUpper = trigger_config->thresholdUpper,
        .thresholdUpperHysteresis = hysteresis,
        .thresholdLower = trigger_config->thresholdLower,
        .thresholdLowerHysteresis = hysteresis
    };
    PICO_STATUS status = ps6000aSetTriggerChannelProperties(handle, &channelProperty, nChannelProperties, 0, trigger_config->autoTriggerMicroSeconds);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelProperties", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

uint32_t is_Channel_On(enum Channel channel){

    switch (channel)
    {
    case CHANNEL_A:
        return channel_status.channel_a;
        break;
    case CHANNEL_B:
        return channel_status.channel_b;
        break;
    case CHANNEL_C:
        return channel_status.channel_c;
        break;
    case CHANNEL_D:
        return channel_status.channel_d;
        break;
    default:
        return -1;
    }
}

/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * 
 * @param waveform_buffer Pointer to the buffer where the waveform data will be stored.
 * @param channel_config Pointer to the ChannelConfigs structure containing channel-specific settings.
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS set_data_buffer(int16_t* waveform_buffer[CHANNEL_NUM], struct ChannelConfigs* channel_config[CHANNEL_NUM], struct SampleConfigs* sample_config) {
    PICO_STATUS status = ps6000aSetDataBuffer(
        handle, (PICO_CHANNEL)NULL, NULL, 0, PICO_INT16_T, 0, 0, 
        PICO_CLEAR_ALL      // Clear buffer in Picoscope buffer list
    );
    for (size_t i = 0; i < CHANNEL_NUM; i++)
    {
        if (status != PICO_OK) {
            log_error("ps6000aSetDataBuffer PICO_CLEAR_ALL", status, __FILE__, __LINE__);
        }
        if (is_Channel_On(channel_config[i]->channel))
        {
            status = ps6000aSetDataBuffer(
                handle, 
                channel_config[i]->channel, 
                waveform_buffer[i], 
                sample_config->num_samples, 
                PICO_INT16_T, 
                0, 
                sample_config->down_sample_ratio_mode, 
                PICO_ADD
            );
            
            if (status != PICO_OK) {
                log_error("ps6000aSetDataBuffer PICO_ADD", status, __FILE__, __LINE__);
            }
        }
    }
    

    return status;
}

PICO_STATUS set_AUX_trigger(struct TriggerConfigs* trigger_config) {
    PICO_STATUS status = set_trigger_conditions(trigger_config);
    if (status != PICO_OK) return status;

    status = set_trigger_directions(trigger_config);
    if (status != PICO_OK) return status;

    status = set_trigger_properties(trigger_config);
    if (status != PICO_OK) return status;

    return status;
}

/**
 * Initiates a block capture on the Picoscope device.
 * 
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @param time_indisposed_ms Pointer to a variable where the time indisposed (in milliseconds) will be stored.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS run_block_capture(struct SampleConfigs* sample_config, double* time_indisposed_ms, uint8_t* capturing) {
    PICO_STATUS status = 0;

    status = start_block_capture(sample_config, time_indisposed_ms);
    if (status != PICO_OK) {
        log_error("start_block_capture", status, __FILE__, __LINE__);
        return status;
    }

    status = wait_for_capture_completion(sample_config, capturing);
    if (status != PICO_OK) {
        log_error("wait_for_capture_completion", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

void ps6000aBlockReadyCallback(int16_t handle, PICO_STATUS status, void *pParameter)
{
    BlockCaptureState *state = (BlockCaptureState *)pParameter;
    state->callbackStatus = status;
    state->dataReady = 1;
}

/**
 * Initiates a block capture on the Picoscope device.
 * 
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @param timebase The timebase value to use for the capture.
 * @param time_indisposed_ms Pointer to a variable where the time indisposed (in milliseconds) will be stored.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS start_block_capture(struct SampleConfigs* sample_config, double* time_indisposed_ms) {
    uint64_t pre_trigger_samples = (uint64_t)sample_config->num_samples * sample_config->trigger_position_ratio;
    uint64_t post_trigger_samples = sample_config->num_samples - pre_trigger_samples;

    free(callback_state);
    callback_state = (BlockCaptureState*)malloc(sizeof(BlockCaptureState));
    if (callback_state == NULL) {
        log_error("BlockCaptureState malloc", PICO_MEMORY_FAIL, __FILE__, __LINE__);
        return PICO_MEMORY_FAIL;
    }
        memset(callback_state, 0, sizeof(BlockCaptureState)); // Initialize to zero

    PICO_STATUS status = ps6000aRunBlock(
        handle,
        pre_trigger_samples,    
        post_trigger_samples,
        sample_config->timebase_configs.timebase,
        time_indisposed_ms,
        0,
        ps6000aBlockReadyCallback, 
        (void*) callback_state
    );
    if (status != PICO_OK) {
        log_error("ps6000aRunBlock", status, __FILE__, __LINE__);
    }

    return status;
}


/**
 * Waits for the block capture to complete by polling the Picoscope device.
 * 
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS wait_for_capture_completion(struct SampleConfigs* sample_config, uint8_t* capturing)
{
    PICO_STATUS status = 0;

    while (!callback_state->dataReady)
    {
        if (!*capturing) {
            ps6000aStop(handle);
            sample_config->num_samples = 0;
            printf("Capture stopped.\n");     
            return status;
        }
        printf("%d",status);
        usleep(1000); // Sleep 1 ms to avoid busy-waiting
    }

    ps6000aStop(handle);

    if (callback_state->callbackStatus != PICO_OK)
    {
        log_error("wait_for_capture_completion", status, __FILE__, __LINE__);
        return status;
    }

    printf("Capture finished.\n");

    status = retrieve_waveform_data(sample_config);
    if (status != PICO_OK) {
        return status;
    }  

    return callback_state->callbackStatus;
}

// PICO_STATUS wait_for_capture_completion(struct SampleConfigs* sample_config, uint8_t* capturing) {
//     PICO_STATUS status = 0;
//     int16_t trigger_status = 0;
//     while (1) {
//         status = ps6000aIsReady(handle, &trigger_status);
//         if (status != PICO_OK) {
//             log_error("ps6000aIsReady", status, __FILE__, __LINE__);
//             return status;
//         }

//         if (trigger_status == 1) {
//             printf("Capture finished.\n");

//             status = retrieve_waveform_data(sample_config);
//             if (status != PICO_OK) {
//                 return status;
//             }
//             break;
//         }
//         if (!*capturing) {
//             ps6000aStop(handle);
//             sample_config->num_samples = 0;
//             printf("Capture stopped.\n");     
//             break;
//         }
//         usleep(10000);
//     }

//     return status;
// }

/**
 * Retrieves the captured waveform data from the Picoscope device and stores it in the provided buffer.
 * 
 * @param sample_config Pointer to the SampleConfigs structure containing sample-collection settings.
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS retrieve_waveform_data(struct SampleConfigs* sample_config) {
    uint64_t start_index = 0;
    uint64_t segment_index = 0;
    int16_t overflow = 0;
    PICO_STATUS status = ps6000aGetValues(
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