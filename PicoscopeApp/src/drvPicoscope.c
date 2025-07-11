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
#include <dbAccess.h>

#include "drvPicoscope.h"
#define MAX_PICO 10
static PS6000AModule *PS6000AModuleList[MAX_PICO] = {NULL};
epicsMutexId epics_ps6000a_call_mutex;
void log_error(char* function_name, uint32_t status, const char* FILE, int LINE){ 
    printf("Error in %s (file: %s, line: %d). Status code: 0x%08X\n", function_name, FILE, LINE, status);
}

/**
 * Opens the Picoscope with specified serial number with the requested resolution. 
 * 
 * @param resolution int16_t The sampling resolution to be used when opening the PicoScope.  
 *                           The following values are valid: 
 *                           - 0: 8-bit resolution
 *                           - 10: 10-bit resolution
 *                           - 1: 12-bit resolution
 * @param serial_num char Poitner The serial number of the device to be opened. 
 * @param handle     int16_t Poitner On exit, the device identifier for future communication. 
 * 
 * @return           PICO_STATUS Return 0 if the device is successfully closed, or a non-zero error code. 
*/
PICO_STATUS open_picoscope(struct PS6000AModule* mp, int16_t* handle){

    int16_t handle_buffer; 
    PICO_STATUS status = ps6000aOpenUnit(&handle_buffer, (int8_t*) mp->serial_num, mp->resolution);
    if (status != PICO_OK) 
    { 
        mp->status = 0;
        log_error("ps6000aOpenUnit", status, __FILE__, __LINE__); 
        return status;  
    }
    mp->status = 1;
    epics_ps6000a_call_mutex = epicsMutexCreate();
    *handle = handle_buffer;
    return PICO_OK;
}

/**
 * Close an open PicoScope device.
 *
 * @param  handle int16_t handle The device identifier returned by open_picoscope(). 
 * 
 * @return        PICO_STATUS Return 0 if the device is successfully closed, or a non-zero error code. 
*/
PICO_STATUS close_picoscope(struct PS6000AModule* mp){ 
    PICO_STATUS status;
    for(size_t channel_index = 0; channel_index < NUM_CHANNELS; channel_index++){
        status = set_channel_off(
            mp->channel_configs[channel_index].channel, 
            mp->handle,
            &mp->channel_status
        );
        if (status != PICO_OK) 
        { 
            mp->status = 0;
            log_error("ps6000aCloseUnit set_channel_off", status, __FILE__, __LINE__);
            return status;  
        }
    }
    epicsMutexLock(epics_ps6000a_call_mutex);
    status = ps6000aCloseUnit(mp->handle);
    epicsMutexUnlock(epics_ps6000a_call_mutex);
    if (status != PICO_OK) 
    { 
        mp->status = 0;
        log_error("ps6000aCloseUnit", status, __FILE__, __LINE__);
        return status;  
    }
    //epicsMutexDestroy(epics_ps6000a_call_mutex);
    mp->status = 1;
    return PICO_OK;
}

/**
 * Check that open device is still connected. 
 * 
 * @param handle int16_t The device identifier returned by open_picoscope(). 
 * 
 * @return       PICO_STATUS Return 0 if device is connect, otherwise a non-zero error code.
*/
PICO_STATUS ping_picoscope(struct PS6000AModule* mp){ 
    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aPingUnit(mp->handle);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    // If driver call in progress, return connected. 
    if (status == PICO_DRIVER_FUNCTION) {
        mp->status = 1;
        return PICO_OK;    
    }
    if (status != PICO_OK) {
        log_error("ps6000aPingUnit. Cannot ping device.", status, __FILE__, __LINE__);
        mp->status = 0;
        return status; 
    }
    mp->status = 1;
    return PICO_OK;
}

/**
 * Set the sample resolution of the currently connected PicoScope. 
 * 
 * @param resolution int16_t The sampling resolution to be used.  
 *                           The following values are valid: 
 *                           - 0: 8-bit resolution
 *                           - 10: 10-bit resolution
 *                           - 1: 12-bit resolution
 * @param handle     int16_t The device identifier returned by open_picoscope(). 
 * 
 * 
 * @return           PICO_STATUS Return 0 if resolution successfully set, otherwise a non-zero error code.
*/
PICO_STATUS set_resolution(int16_t resolution, int16_t handle){ 
   
    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aSetDeviceResolution(handle, resolution); 
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK){ 
        log_error("ps6000aSetDeviceResolution", status, __FILE__, __LINE__);
        return status;
    }

    return PICO_OK; 
}

/**
 * Get the sample resolution of the currently connected PicoScope. 
 * 
 * @param resolution int16_t Pointer On exit, the sampling resolution of the device. 
 * @param handle     int16_t The device identifier returned by open_picoscope(). 
 * 
 * @return           PICO_STATUS Return 0 if resolution returned, otherwise a non-zero error code.
*/
PICO_STATUS get_resolution(int16_t* resolution, int16_t handle) {

    PICO_DEVICE_RESOLUTION device_resolution; 
    
    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aGetDeviceResolution(handle, &device_resolution); 
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if(status != PICO_OK) {
        log_error("ps6000aGetDeviceResolution", status, __FILE__, __LINE__);
        return status;
    }
    *resolution = (int16_t)device_resolution; 
    return PICO_OK; 
}

/**
 * Retrieves the serial number of the currently connected PicoScope. 
 * 
 * @param serial_num Pointer to a pointer of int8_t that will hold the dynamically allocated address of
 *                             the memory buffer containing the serial number. 
 * @param handle     int16_t The device identifier returned by open_picoscope(). 
 * 
 * @return           PICO_STATUS Return 0 if the serial number is successfully retrieved, otherwise a non-zero error code.
*/
PICO_STATUS get_serial_num(int8_t** serial_num, int16_t handle) {

    int16_t required_size = 0; 

    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_BATCH_AND_SERIAL);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    int8_t* serial_num_buffer = calloc(required_size, sizeof(int8_t)); 

    epicsMutexLock(epics_ps6000a_call_mutex);
    status = ps6000aGetUnitInfo(handle, serial_num_buffer, required_size, &required_size, PICO_BATCH_AND_SERIAL);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    *serial_num = serial_num_buffer;


    return PICO_OK;  
}


/**
 * Retrieves the model number of the currently connected PicoScope. 
 * 
 * @param model_num Pointer to a pointer of int8_t that will hold the dynamically allocated address of
 *                          the memory buffer containing the model number. 
 * @param handle    int16_t The device identifier returned by open_picoscope(). 
 * 
 * @return          PICO_STATUS Return 0 if the model number is successfully retrieved, otherwise a non-zero error code.
*/
PICO_STATUS get_model_num(int8_t** model_num, int16_t handle) {

    int16_t required_size = 0; 

    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_VARIANT_INFO);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    int8_t* model_num_buffer = calloc(required_size, sizeof(int8_t));

    
    epicsMutexLock(epics_ps6000a_call_mutex);
    status = ps6000aGetUnitInfo(handle, model_num_buffer, required_size, &required_size, PICO_VARIANT_INFO);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aGetUnitInfo", status, __FILE__, __LINE__);
        return status;  
    }

    *model_num = model_num_buffer;
    
    return PICO_OK;
}


/**
 * Retrieves the model and serial number of the currently connected PicoScope.
 *
 * @param device_info Pointer to a pointer of int8_t (String pointer) that will store the address of a dynamically allocated
 *                            memory buffer containing the device information (e.g., model and serial number).
 *                            The caller is responsible for freeing this memory.
 * @param handle      int16_t The device identifier returned by open_picoscope().
 * @return            PICO_STATUS Returns 0 if the device information is successfully retrieved, otherwise a non-zero error code.
 */
PICO_STATUS get_device_info(int8_t** device_info, int16_t handle) {

    int8_t* serial_num = NULL;
    int8_t* model_num = NULL; 

    PICO_STATUS status = get_serial_num(&serial_num, handle);
    if (status == PICO_OK) {
        status = get_model_num(&model_num, handle); 
        if (status == 0) {
            const static char* FORMAT_STR = "Picoscope %s [%s]";
            int16_t required_size = snprintf(NULL, 0, FORMAT_STR, model_num, serial_num) + 1; 

            int8_t* device_info_buffer = malloc(required_size);
            snprintf((char*)device_info_buffer, required_size, FORMAT_STR, model_num, serial_num);
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
 * @param channel        ChannelConfigs structure that contains the configuration to be activated. 
 *                                      The structure holds the coupling type, voltage range, analog offset, and 
 *                                      bandwidth to configure the channel. 
 * @param handle         int16_t The device identifier returned by open_picoscope(). 
 * @param channel_status EnabledChannelFlags Pointer On exit, the updated status of Picoscope channels.
 * 
 * @return               PICO_STATUS Return PICO_OK if the channel is succesfully set on, otherwise a non-zero error code. 
*/
PICO_STATUS set_channel_on(struct ChannelConfigs channel, int16_t handle, EnabledChannelFlags* channel_status) {

    epicsMutexLock(epics_ps6000a_call_mutex);
    uint32_t status = ps6000aSetChannelOn(handle, channel.channel, channel.coupling, channel.range, channel.analog_offset, channel.bandwidth);
    epicsMutexUnlock(epics_ps6000a_call_mutex);
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
            return PICO_INVALID_CHANNEL;
    }

    printf("Setting channel %d on.\n", channel.channel);
  
    return PICO_OK;

}

/**
 * Deactivates the specified channel on the connected Picoscope. 
 * 
 * @param channel        int The channel to be shut off. 
 *                           The following values are valid: 
 *                             - 0: Channel A
 *                             - 1: Channel B 
 *                             - 2: Channel C
 *                             - 3: Channel D
 * @param handle         int16_t The device identifier returned by open_picoscope(). 
 * @param channel_status EnabledChannelFlags Pointer On exit, the updated status of Picoscope channels. 
 *  
 * @return               PICO_STATUS Return PICO_OK if the channel is successfully turned off, otherwise a non-zero error code.
*/
PICO_STATUS set_channel_off(int channel, int16_t handle, EnabledChannelFlags* channel_status) {
    epicsMutexLock(epics_ps6000a_call_mutex);

    uint32_t status = ps6000aSetChannelOff(handle, channel);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

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
            return PICO_INVALID_CHANNEL;
    }

    printf("Set channel %d off.\n", channel);
    return PICO_OK;
}

/**
 * Get the status (on/off) of specified channel. 
 * 
 * @param channel        int16_t The channel you wish to check the status of. 
 *                               The following values are valid: 
 *                               - 0: Channel A
 *                               - 1: Channel B 
 *                               - 2: Channel C
 *                               - 3: Channel D 
 * @param channel_status EnabledChannelFlags Channels currently enabled on the device identified by the handle. 
 * @return               uint32_t Return 1 if the channel is enabled, 0 if not, and -1 if channel does not exist
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
 * @param channel           ChannelConfigs structure that contains the configurations of the channel in question. 
 * @param handle            int16_t The device identifier returned by open_picoscope(). 
 * @param max_analog_offset double Pointer On exit, the max analog offset voltage allowed for the range. 
 * @param min_analog_offset double Pointer On exit, the min analog offset voltage allowed for the range. 
 * 
 * @return                  PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS get_analog_offset_limits(struct ChannelConfigs channel, int16_t handle, double* max_analog_offset, double* min_analog_offset){

    double maximum_voltage; 
    double minimum_voltage;

    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aGetAnalogueOffsetLimits(handle, channel.range, channel.coupling, &maximum_voltage, &minimum_voltage); 
    epicsMutexUnlock(epics_ps6000a_call_mutex);
    if (status != PICO_OK)
    {
        log_error("ps6000aGetAnalogueOffsetLimits", status, __FILE__, __LINE__);
        return status;
    }

    *max_analog_offset = maximum_voltage;
    *min_analog_offset = minimum_voltage;

    return PICO_OK; 
}

/**
 * Uses a requested sample interval to determine the closest timebase and sample interval that can 
 * be applied to the connected Picoscope given the resolution and number of channels enabled. 
 * 
 * @param requested_time_interval double The requested sample interval in seconds. 
 * @param handle                  int16_t The device identifier returned by open_picoscope(). 
 * @param channel_status          EnabledChannelFlags Channels currently enabled on the device identified by the handle. 
 * @param timebase                uint32_t Pointer On exit, the value of the closest timebase for the requested interval. 
 * @param available_time_interval double Pointer On exit, the closests sample interval available, given the device configurations, 
 *                                to the request interval. 
 * 
 * @return                        PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS validate_sample_interval(double requested_time_interval, int16_t handle, EnabledChannelFlags channel_status, uint32_t* timebase, double* available_time_interval){

    uint32_t enabledChannels = *(uint32_t*)&channel_status;

    int16_t resolution = 0; 
    PICO_STATUS status = get_resolution(&resolution, handle);
    if (status != PICO_OK) {
        return status;
    }

    uint32_t timebase_return; 
    double time_interval_available;

    epicsMutexLock(epics_ps6000a_call_mutex);
    status = ps6000aNearestSampleIntervalStateless(handle, enabledChannels, requested_time_interval, resolution, &timebase_return, &time_interval_available); 
    epicsMutexUnlock(epics_ps6000a_call_mutex);
    
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


    return PICO_OK; 
} 


/** 
 * Converts a time in some unit to seconds. 
 * 
 * @param  time double An amount of time. 
 * @param  unit UnitPerDiv Enum The unit used for time.  
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
    if (num_division == 0){
        printf("ERROR: calculate_samples_per_division num_division is 0\n");
    }
    return (double) (num_samples / num_division);
}

double calculate_sample_interval(double secs_per_div, double samples_per_div){ 
    if (samples_per_div == 0){
        printf("ERROR: calculate_sample_interval samples_per_div is 0\n");
    }
    return secs_per_div / samples_per_div;    
}

double calculate_sample_rate(double available_sample_interval) {
    if (available_sample_interval == 0){
        printf("ERROR: calculate_sample_rate available_sample_interval is 0\n");
    }
    return 1 / available_sample_interval; 
}

double calculate_num_samples(double secs_per_div, int16_t num_divisions, double time_interval_secs) {
    if (time_interval_secs == 0){
        printf("ERROR: calculate_num_samples time_interval_secs is 0\n");
    }
    return secs_per_div * num_divisions /  time_interval_secs;
}

/**
 *  Gets the valid timebase configs given the requested time per division, number of divisions, and number of samples. 
 * 
 * @param mp              PS6000AModule Pointer to the PS6000AModule structure containing Picoscope timebase configurations. 
 * @param sample_interval double Pointer On exit, the interval at which samples will be taken in seconds. 
 * @param timebase        uint32_t Pointer On exit, the timebase for the requested time per division. 
 * @param sample_rate     double Pointer On exit, the sample rate for the request time per division. 
 * 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS get_valid_timebase_configs(struct PS6000AModule* mp, double* sample_interval, uint32_t* timebase, double* sample_rate) {

    uint32_t available_timebase; 
    double available_sample_interval;
    PICO_STATUS status;
    double secs_per_div = convert_to_seconds(
        mp->sample_config.timebase_configs.time_per_division, 
        mp->sample_config.timebase_configs.time_per_division_unit
    );
    mp->sample_config.num_samples = mp->sample_config.unadjust_num_samples; 


    double samples_per_division = calculate_samples_per_division(
        mp->sample_config.num_samples, 
        mp->sample_config.timebase_configs.num_divisions
    );

    double requested_sample_interval = calculate_sample_interval(secs_per_div, samples_per_division); 
    double target_sample_interval = requested_sample_interval;

    // Sample interval should be less than the pulse width of AuxIO trigger signal 
    // set by the user to avoid missed triggers. If requested sample interval is 
    // larger, use AUX trigger pulse width as the target sample interval. 
    if (requested_sample_interval > mp->trigger_config.AUXTriggerSignalPulseWidth) {
        target_sample_interval = mp->trigger_config.AUXTriggerSignalPulseWidth;
    }

    status = validate_sample_interval(
        target_sample_interval, 
        mp->handle, 
        mp->channel_status, 
        &available_timebase, 
        &available_sample_interval
    );
    if (status != PICO_OK) {
        return status; 
    }

    // If the requested sample interval was adjusted, find number of samples 
    // required at the set timebase to achieve the target sample interval. 
    if (target_sample_interval != requested_sample_interval){
        mp->sample_config.num_samples = calculate_num_samples(
            secs_per_div, 
            mp->sample_config.timebase_configs.num_divisions, 
            available_sample_interval
        );
        
        samples_per_division = calculate_samples_per_division(
            mp->sample_config.num_samples, 
            mp->sample_config.timebase_configs.num_divisions
        );
    }
    if (mp->sample_config.num_samples > mp->waveform_size){
        mp->subwaveform_num = (mp->sample_config.num_samples + mp->waveform_size - 1) / mp->waveform_size;
        mp->sample_config.subwaveform_samples_num = mp->waveform_size;
        mp->sample_config.original_subwaveform_samples_num = mp->waveform_size;
    }else{
        mp->subwaveform_num = 0;
        mp->sample_config.subwaveform_samples_num = 0;
        mp->sample_config.original_subwaveform_samples_num = 0;

    }
    *sample_rate = calculate_sample_rate(available_sample_interval);
    *sample_interval = available_sample_interval; 
    *timebase = available_timebase;

    return PICO_OK;
}


/** 
 * The following is to convert the enum values from $(OSC):CH$(CHANNEL):range to
 * the corresponding voltage. 
 * */
const double ranges_in_volts[] = {
    0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20 
};
double get_range(int index) {

    if (index >= 0 && index < sizeof(ranges_in_volts) / sizeof(ranges_in_volts[0])) {
        return ranges_in_volts[index];
    }

    return -1; 
}

/** 
 * Calculates the scaled value from a given voltage at a specified voltage range. 
 * 
 * @param volts The value to be scaled in voltages. 
 *        voltage_range The current voltage range. 
 *        scaled_value On exit, the scaled value. 
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return PICO_STATUS PICO_OK if successful, other a non-zero status code. 
 * */ 
PICO_STATUS calculate_scaled_value(double volts, int16_t voltage_range, int16_t* scaled_value, int16_t handle){ 
    
    double range_in_volts = get_range(voltage_range); 
    
    if (range_in_volts < volts){
        printf("Voltage is outside of range.\n"); 
        return PICO_THRESHOLD_OUT_OF_RANGE; 
    }

    int16_t min, max; 
    uint32_t status = get_adc_limits(&min, &max, handle);
    if (status != PICO_OK) {
        return status; 
    }

    int16_t scaled = ((double) volts / range_in_volts) * max;
    printf("(%f / %f) * %d = %d \n", volts, range_in_volts, max, scaled); 
    
    *scaled_value = scaled;     

    return status; 
}

/**
 * Gets the maximum and minimun sample values that the ADC of the connected picoscope can produce 
 * at the resolution set. 
 * 
 * @param min_val On exit, the minimum sample value. 
 *        max_val On exit, the maximum sample value. 
 *        handle The device identifier returned by open_picoscope(). 
 * 
 * @return PICO_STATUS PICO_OK if successful, other a non-zero status code. 
 * */
PICO_STATUS get_adc_limits(int16_t* min_val, int16_t* max_val, int16_t handle){ 

    int16_t min, max, resolution;

    uint32_t status = get_resolution(&resolution, handle); 
    if (status != PICO_OK) {
        return status; 
    }

    epicsMutexLock(epics_ps6000a_call_mutex);
    status = ps6000aGetAdcLimits(handle, resolution, &min, &max);
    epicsMutexUnlock(epics_ps6000a_call_mutex);
    if (status != PICO_OK){ 
        log_error("ps6000aGetAdcLimits", status, __FILE__, __LINE__); 
        return status; 
    }

    *min_val = min; 
    *max_val = max; 

    return status;
}



typedef struct {
    PICO_STATUS callbackStatus; // Status from the callback
    int dataReady;
    struct PS6000AModule* mp;
} BlockReadyCallbackParams;
PICO_STATUS setup_picoscope(struct PS6000AModule* mp);
PICO_STATUS init_block_ready_callback_params(struct PS6000AModule* mp);
PICO_STATUS run_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms);
PICO_STATUS set_data_buffer(struct PS6000AModule* mp);
PICO_STATUS apply_trigger_configurations(struct PS6000AModule* mp);
PICO_STATUS start_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms);
PICO_STATUS wait_for_capture_completion(struct PS6000AModule* mp);
PICO_STATUS retrieve_waveform_data(struct PS6000AModule* mp);
PICO_STATUS update_trigger_timing_info(struct PS6000AModule* mp, uint64_t segment_index);
void free_subwaveforms(struct PS6000AModule* mp, uint16_t subwaveform_num);


BlockReadyCallbackParams* blockReadyCallbackParams;

/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing Picoscope configurations. 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS setup_picoscope(struct PS6000AModule* mp) {

    PICO_STATUS status = 0;
    status = init_block_ready_callback_params(mp);
    if (status != PICO_OK) {
        return status;
    }

    if (mp->trigger_config.triggerType == NO_TRIGGER) {
        // If no trigger set, clear previous triggers and do not set new one 
        PICO_CONDITION condition;
        epicsMutexLock(epics_ps6000a_call_mutex);
        status = ps6000aSetTriggerChannelConditions(mp->handle, &condition, 0, PICO_CLEAR_ALL);   
        epicsMutexUnlock(epics_ps6000a_call_mutex);

        if (status != PICO_OK) {
            log_error("ps6000aSetTriggerChannelConditions", status, __FILE__, __LINE__);
            return status;
        }        
    } 
    else { 
        status = apply_trigger_configurations(mp);

        if (status != PICO_OK) {
            log_error("apply_trigger_configurations", status, __FILE__, __LINE__);
            return status;
        }
    }

    status = set_data_buffer(mp);

    if (status != PICO_OK) {
        return status;
    }
    return status;
}


/**
 * Apply the trigger condition configurations.
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing Picoscope trigger configurations. 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS set_trigger_conditions(struct PS6000AModule* mp) {
    int16_t nConditions = 1;
    PICO_STATUS status = 0;

    PICO_CONDITION condition = {
        .source = mp->trigger_config.channel,
        .condition = PICO_CONDITION_TRUE
    };
    epicsMutexLock(epics_ps6000a_call_mutex);
    ps6000aSetTriggerChannelConditions(mp->handle, &condition, nConditions, PICO_CLEAR_ALL);
    ps6000aSetTriggerChannelConditions(mp->handle, &condition, nConditions, PICO_ADD);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelConditions", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

/**
 * Apply the trigger direction configurations.
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing Picoscope trigger configurations. 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS set_trigger_directions(struct PS6000AModule* mp) {
    int16_t nDirections = 1;    // Only support one now 
    PICO_STATUS status = 0;
    if(mp->trigger_config.thresholdDirection == 10){
        mp->trigger_config.thresholdDirection = PICO_NEGATIVE_RUNT;
    }
    PICO_DIRECTION direction = {
        .channel = mp->trigger_config.channel,
        .direction = mp->trigger_config.thresholdDirection,
        .thresholdMode = mp->trigger_config.thresholdMode
    };
    epicsMutexLock(epics_ps6000a_call_mutex);
    ps6000aSetTriggerChannelDirections(mp->handle, &direction, nDirections);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelDirections", status, __FILE__, __LINE__);
        return status;
    }
    return status;
}

/**
 * Apply the trigger propertie configurations.
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing Picoscope trigger configurations. 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS set_trigger_properties(struct PS6000AModule* mp) {
    int16_t nChannelProperties = 1; // Only support one now 
    
    uint16_t hysteresis_upper = (uint16_t)((UINT16_MAX / 100.0) * mp->trigger_config.thresholdUpperHysteresis);  
    uint16_t hysteresis_lower = (uint16_t)((UINT16_MAX / 100.0) * mp->trigger_config.thresholdLowerHysteresis);  
   
    PICO_TRIGGER_CHANNEL_PROPERTIES channelProperty = { 
        .channel = mp->trigger_config.channel,
        .thresholdUpper = mp->trigger_config.thresholdUpper,
        .thresholdUpperHysteresis = hysteresis_upper,
        .thresholdLower = mp->trigger_config.thresholdLower,
        .thresholdLowerHysteresis = hysteresis_lower
    };
    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aSetTriggerChannelProperties(mp->handle, &channelProperty, nChannelProperties, 0, 0);
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (status != PICO_OK) {
        log_error("ps6000aSetTriggerChannelProperties", status, __FILE__, __LINE__);
        return status;
    }

    return status;
}

/**
 * Apply the trigger configurations.
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing Picoscope trigger configurations. 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS apply_trigger_configurations(struct PS6000AModule* mp) {
    PICO_STATUS status;
    printf("Trigger channel %d\n", mp->trigger_config.channel);
    printf("Trigger type %d\n", mp->trigger_config.triggerType);
    printf("Trigger direction %d, Trigger mode %d\n", mp->trigger_config.thresholdDirection, mp->trigger_config.thresholdMode);
    printf("Trigger Upper Threshold %d, Trigger Lower Threshold %d\n", mp->trigger_config.thresholdUpper, mp->trigger_config.thresholdLower);
    printf("Trigger Upper Hysteresis %d, Trigger Lower Hysteresis %d\n", mp->trigger_config.thresholdUpperHysteresis, mp->trigger_config.thresholdLowerHysteresis);
    printf("Trigger Position Ratio %f\n", mp->sample_config.trigger_position_ratio);
    status = set_trigger_conditions(mp);
    if (status != PICO_OK) return status;

    status = set_trigger_directions(mp);
    if (status != PICO_OK) return status;

    status = set_trigger_properties(mp);
    if (status != PICO_OK) return status;

    return status;
}

/**
 * Configures the data buffer for the specified channel on the Picoscope device.
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing data acquisition settings and configurations. 
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS set_data_buffer(struct PS6000AModule* mp) {

    epicsMutexLock(epics_ps6000a_call_mutex);
    PICO_STATUS status = ps6000aSetDataBuffer(
        mp->handle, (PICO_CHANNEL)NULL, NULL, 0, PICO_INT16_T, 0, 0, 
        PICO_CLEAR_ALL      // Clear buffer in Picoscope buffer list
    );
    if (status != PICO_OK) {
        log_error("ps6000aSetDataBuffer PICO_CLEAR_ALL", status, __FILE__, __LINE__);
        epicsMutexUnlock(epics_ps6000a_call_mutex);
        return status;
    }
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (mp->subwaveform_num > 0)
    {
        mp->sample_config.subwaveform_samples_num = mp->sample_config.original_subwaveform_samples_num;
        switch (mp->sample_config.down_sample_ratio_mode) {
            case RATIO_MODE_AVERAGE:
                mp->sample_config.subwaveform_samples_num /= mp->sample_config.down_sample_ratio;
                break;
            case RATIO_MODE_DECIMATE:
                mp->sample_config.subwaveform_samples_num /= mp->sample_config.down_sample_ratio;
                break;
            default:
                break;
        }

        for (size_t i = 0; i < NUM_CHANNELS; i++){
		    mp->streamWaveformBuffers[i] = (int16_t**)calloc(mp->subwaveform_num, sizeof(int16_t*));
            for (int j = 0; j < mp->subwaveform_num; j++) {
		        mp->streamWaveformBuffers[i][j] = (int16_t*)calloc(mp->sample_config.subwaveform_samples_num, sizeof(int16_t));
	        }
            if (get_channel_status(i, mp->channel_status)){
                epicsMutexLock(epics_ps6000a_call_mutex);
	            status = ps6000aSetDataBuffer(
                    mp->handle,
                    mp->channel_configs[i].channel,
                    mp->streamWaveformBuffers[i][0],
                    mp->sample_config.subwaveform_samples_num,
                    PICO_INT16_T,
                    0,
                    mp->sample_config.down_sample_ratio_mode, 
                    PICO_ADD
                );
                epicsMutexUnlock(epics_ps6000a_call_mutex);
                if (status != PICO_OK) {
                    log_error("ps6000aSetDataBuffer subwaveform_num > 0", status, __FILE__, __LINE__);
                }
            }
        }
    }else{
        for (size_t i = 0; i < NUM_CHANNELS; i++)
        {
            if (get_channel_status(mp->channel_configs[i].channel, mp->channel_status)){
                epicsMutexLock(epics_ps6000a_call_mutex);
                status = ps6000aSetDataBuffer(
                    mp->handle, 
                    mp->channel_configs[i].channel, 
                    mp->waveform[i],
                    mp->sample_config.num_samples, 
                    PICO_INT16_T, 
                    0, 
                    mp->sample_config.down_sample_ratio_mode, 
                    PICO_ADD
                );
                epicsMutexUnlock(epics_ps6000a_call_mutex);
                if (status != PICO_OK) {
                    log_error("ps6000aSetDataBuffer subwaveform_num = 0", status, __FILE__, __LINE__);
                }
            }
        }
    }
    
    

    return status;
}

typedef struct ChannelStreamingArg 
{
    enum Channel channel;
    struct PS6000AModule* mp;
} ChannelStreamingArg;

void channel_streaming_thread_function(void *arg){
    ChannelStreamingArg* ap = (struct ChannelStreamingArg *)arg;
    PS6000AModule* mp = ap->mp;
    enum Channel channel_index = ap->channel;
    int buffer_index = 0;
    int triggered_flag = 0;
    int set_buffer_retry = 0;
	PICO_STREAMING_DATA_INFO streamData;
    PICO_STREAMING_DATA_TRIGGER_INFO streamTrigger;
    PICO_STATUS status = PICO_OK;
    streamData.bufferIndex_ = 0;
    streamData.channel_ = channel_index;
    streamData.mode_ = mp->sample_config.down_sample_ratio_mode;
    streamData.noOfSamples_ = 0;
    streamData.overflow_ = 0;
    streamData.startIndex_ = 0;
    streamData.type_ = PICO_INT16_T;
    uint16_t subwaveform_num = mp->subwaveform_num;
    uint64_t subwaveform_samples_num = mp->sample_config.subwaveform_samples_num;
    if (mp->trigger_config.triggerType == NO_TRIGGER){ 
        triggered_flag = 1;
    } 
    while (buffer_index < subwaveform_num) {
        epicsMutexLock(mp->epics_acquisition_flag_mutex);
        if (*mp->dataAcquisitionFlag == 0)
        {
            epicsEventSignal(mp->channelStreamingFinishedEvents[channel_index]);
            epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
            return;
        }
        epicsMutexUnlock(mp->epics_acquisition_flag_mutex);

        if (streamData.startIndex_ + streamData.noOfSamples_ == subwaveform_samples_num){
            do
            {
                epicsMutexLock(epics_ps6000a_call_mutex);
                status = ps6000aSetDataBuffer(
                    mp->handle,
                    channel_index,
                    mp->streamWaveformBuffers[channel_index][buffer_index],
                    subwaveform_samples_num,
                    PICO_INT16_T,
                    0,
                    mp->sample_config.down_sample_ratio_mode,
                    PICO_ADD
                    );
                epicsMutexUnlock(epics_ps6000a_call_mutex);
                set_buffer_retry ++;
            } while (status == PICO_DRIVER_FUNCTION && set_buffer_retry < 100);
            set_buffer_retry = 0;
            if(status != PICO_OK){
                log_error("ps6000aSetDataBuffer", status, __FILE__, __LINE__);
                return;
            }
        }

        usleep(5000);       // Sleep to give Picoscope device time to collect data
        epicsMutexLock(epics_ps6000a_call_mutex);
        status = ps6000aGetStreamingLatestValues(mp->handle, &streamData, 1, &streamTrigger); // Get the latest values
        epicsMutexUnlock(epics_ps6000a_call_mutex);
        //     /* code */
        // printf("CH %d, valid buffer %d, status %d, numSamples %ld, startIdx %d, sampleCollected %d, total buffer %ld, triggered %d, subwaveform_num: %d\n",
        //         channel_index, buffer_index, status,
        //         subwaveform_samples_num, streamData.startIndex_, streamData.noOfSamples_,
        //         streamData.bufferIndex_, triggered_flag, subwaveform_num);

        if (triggered_flag == 0 && streamTrigger.triggered_ == 0 )
        {
            continue;
        }else{
            triggered_flag =  1;
        }
        if (streamData.startIndex_ + streamData.noOfSamples_ == subwaveform_samples_num){
        
            if(triggered_flag== 0 && streamTrigger.triggered_== 0){
                // Do not use the data if it is not triggered
                continue;
            }

            // If buffers full move to next buffer
            *mp->sample_collected = subwaveform_samples_num;
            int16_t* src = mp->streamWaveformBuffers[channel_index][buffer_index];
            int16_t* dst = mp->waveform[channel_index];

            if (src && dst) {
                memcpy(dst, src, *mp->sample_collected * sizeof(uint16_t));
            }
    

            dbProcess((struct dbCommon *)mp->pRecordUpdateWaveform[channel_index]);
            buffer_index++;
            continue;
        // }else if (status == PICO_WAITING_FOR_DATA_BUFFERS && streamData.startIndex_ == 0 && streamData.noOfSamples_ == 0)
        }else if (status == PICO_WAITING_FOR_DATA_BUFFERS){
            // The Error that can be ignored
            status = PICO_OK;
            continue;
        }else if (status != PICO_OK){
            log_error("ps6000aGetStreamingLatestValues", status, __FILE__, __LINE__);
            return;
        }
        
    }
    epicsEventSignal(mp->channelStreamingFinishedEvents[channel_index]);
}

PICO_STATUS run_stream_capture(struct PS6000AModule* mp){
    PICO_STATUS status;
    double time = mp->sample_config.timebase_configs.sample_interval_secs;
    ChannelStreamingArg channel_streaming_args[NUM_CHANNELS];

    int thread_count = 0;
    status = ps6000aRunStreaming(
        mp->handle,
        &time,
        PICO_S,
        0,
        mp->sample_config.subwaveform_samples_num,
        0,
        mp->sample_config.down_sample_ratio,
        mp->sample_config.down_sample_ratio_mode
        );	// Start continuous streaming
    if (status == PICO_OK) {
        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            if (get_channel_status(mp->channel_configs[i].channel, mp->channel_status)) {
                channel_streaming_args[i].mp = mp;
                channel_streaming_args[i].channel = mp->channel_configs[i].channel;
                mp->channel_streaming_thread_function[i] = epicsThreadCreate("channel_streaming_thread", epicsThreadPriorityMedium,
                    0, (EPICSTHREADFUNC)channel_streaming_thread_function, &channel_streaming_args[i]);
                if (!mp->channel_streaming_thread_function[i]) {
                    log_error("Thread channel_streaming_data_thread creation failed\n", -1, __FILE__, __LINE__);
                    epicsMutexLock(mp->epics_acquisition_flag_mutex);
                    *mp->dataAcquisitionFlag = 0;
                    epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
                    return -1;
                }
                thread_count ++;
            }
        }

	}else{
        log_error("run_stream_capture", status, __FILE__, __LINE__);
        if (status == PICO_BUFFERS_NOT_SET){
            printf("No Channel Opened\n");
        }
        return status;
    }

    for (size_t i = 0; i < thread_count; i++)
    {
        epicsEventWait(mp->channelStreamingFinishedEvents[i]);
    }
    print_time("All Subwaveform Sent");

    status = stop_capturing(mp->handle);
    return status;
}

/**
 * Initialize the blocking mode callback function.
 * @param  mp PS6000AModule Pointer to the PS6000AModule structure containing data ready flag. 
 * @return    PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
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
 * @param mp PS6000AModule Pointer to the PS6000AModule structure containing data acquisition settings and configurations. 
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

/**
 * Defines the blocking mode callback function
 * 
 * @param handle     int16_t The device identifier. 
 * @param status     PICO_STATUS Indicates whether an error occurred during collection of the data.
 * @param pParameter PICO_POINTER Passed from ps6000aRunBlock(). Send if data is ready.
 * @return           PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
void ps6000aBlockReadyCallback(int16_t handle, PICO_STATUS status, PICO_POINTER pParameter)
{
    BlockReadyCallbackParams *state = (BlockReadyCallbackParams *)pParameter;
    state->callbackStatus = status;

    // print_time("ps6000aBlockReadyCallback Trigger Captured");
    // printf("-------------------\n");
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
 * @param mp                 PS6000AModule Pointer The PS6000AModule structure containing sample-collection settings.
 * @param time_indisposed_ms double Pointer Where the time indisposed (in milliseconds) will be stored.
 * @return                   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS start_block_capture(struct PS6000AModule* mp, double* time_indisposed_ms) {
    PICO_STATUS ps6000aRunBlockStatus;
    PICO_STATUS ps6000aStopStatus;
    struct SampleConfigs sample_config = mp->sample_config;
    uint64_t pre_trigger_samples = ((uint64_t)sample_config.num_samples * sample_config.trigger_position_ratio)/100;
    uint64_t post_trigger_samples = sample_config.num_samples - pre_trigger_samples;

    blockReadyCallbackParams->dataReady = 0;
    int8_t runBlockCaptureRetryFlag = 0;

    epicsMutexLock(epics_ps6000a_call_mutex);
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
    // print_time("ps6000aRunBlock Trigger Captured");
    epicsMutexUnlock(epics_ps6000a_call_mutex);

    if (ps6000aRunBlockStatus != PICO_OK) {
        log_error("ps6000aRunBlock", ps6000aRunBlockStatus, __FILE__, __LINE__);
        return ps6000aRunBlockStatus;
    }

    return PICO_OK;
}

/**
 * Waits for the block capture to complete by polling the Picoscope device.
 * 
 * @param mp PS6000AModule Pointer The PS6000AModule structure containing sample-collection settings and EPICS Event id.
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
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
        *mp->sample_collected = 0;
        return PICO_CANCELLED;
    }

    status = retrieve_waveform_data(mp);
    if (status != PICO_OK) {
        return status;
    }  

    return PICO_OK;
}

/**
 * Retrieves the captured waveform data from the Picoscope device and stores it in the provided buffer.
 * 
 * @param mp PS6000AModule Pointer The PS6000AModule structure containing sample-collection settings.
 * @return   PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS retrieve_waveform_data(struct PS6000AModule* mp) {
    uint64_t start_index = 0;
    uint64_t segment_index = 0;
    int16_t overflow = 0;
    uint64_t getValueRetryFlag = 0;
    PICO_STATUS ps6000aStopStatus;
    PICO_STATUS ps6000aGetValuesStatus;

    epicsMutexLock(epics_ps6000a_call_mutex);
    do {

        ps6000aGetValuesStatus = ps6000aGetValues(
            mp->handle,
            start_index,
            mp->sample_collected,
            mp->sample_config.down_sample_ratio,
            mp->sample_config.down_sample_ratio_mode,
            segment_index,
            &overflow
        );   

        if (ps6000aGetValuesStatus == PICO_HARDWARE_CAPTURING_CALL_STOP) {
            getValueRetryFlag++;
            printf("dataReady %d\n",blockReadyCallbackParams->dataReady);
            printf("ps6000aGetValues Retry attempt: %ld\n", getValueRetryFlag);
            ps6000aStopStatus = ps6000aStop(mp->handle);
            if (ps6000aStopStatus != PICO_OK) {
                printf("Error: Failed to stop capture, status: %d\n", ps6000aStopStatus);
                epicsMutexUnlock(epics_ps6000a_call_mutex);
                return ps6000aStopStatus;
            }
        }
    } while (ps6000aGetValuesStatus == PICO_HARDWARE_CAPTURING_CALL_STOP);
    epicsMutexUnlock(epics_ps6000a_call_mutex);
    
    if (mp->trigger_config.triggerType != NO_TRIGGER){
        update_trigger_timing_info(mp, segment_index); 
    } 
    else {
        mp->trigger_timing_info.missed_triggers = 0; 
        mp->trigger_timing_info.trigger_freq_secs = 0; 
        mp->trigger_timing_info.prev_trigger_time = 0; 
    }

    if (ps6000aGetValuesStatus != PICO_OK) {
        log_error("ps6000aGetValues", ps6000aGetValuesStatus, __FILE__, __LINE__);
        return ps6000aGetValuesStatus;
    }


    return PICO_OK;
}


/** 
 * Calculates the time between triggers in seconds using the timestamp counter, missed triggers, and current sample interval. 
 * 
 * @param sample_interval_secs double The rate at which samples are taken from the Picoscope in seconds. 
 *        prev_time_stamp uint64_t    The time in samples at which the previous trigger occurred. 
 *        cur_time_stamp uint64_t     The time in samples at which the most recent trigger occurred. 
 *        missed_triggers uint64_t    The number of triggers missed between prev_time_stamp and cur_time_stamp 
 * 
 * @return double Returns the time betweeen triggers in seconds. 
*/
double calculate_trigger_freqency(double sample_interval_secs, uint64_t prev_time_stamp, uint64_t cur_time_stamp, uint64_t missed_triggers) {
    
    double samples_between_triggers = ((double)cur_time_stamp - prev_time_stamp) / (double) missed_triggers; 
    
    return samples_between_triggers * sample_interval_secs; 

}

/** 
 * Update trigger timing information, such as the trigger frequency, number of missed triggers, 
 * and count of previous trigger. 
 * 
 * @param mp PS6000AModule Pointer The PS6000AModule structure containing trigger information to be updated.
 *        segment_index uint64_t The memory segment containing the data to retrieve trigger timing from. 
 * 
 * @return PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
inline PICO_STATUS update_trigger_timing_info(struct PS6000AModule* mp, uint64_t segment_index) {
    
    PICO_TRIGGER_INFO triggerInfo[1];        
    uint32_t status = ps6000aGetTriggerInfo(mp->handle, triggerInfo, segment_index, 1);
    if (status != PICO_OK) {
        log_error("ps6000aGetTriggerInfo", status, __FILE__, __LINE__);    
        return status; 
    }

    // Ensure previous trigger time is from current capture before calculating trigger frequency
    if (mp->trigger_timing_info.prev_trigger_time != 0 ){
        mp->trigger_timing_info.trigger_freq_secs = calculate_trigger_freqency(
            mp->sample_config.timebase_configs.sample_interval_secs,
            mp->trigger_timing_info.prev_trigger_time,
            triggerInfo[0].timeStampCounter,
            triggerInfo[0].missedTriggers
        );     
    }
    
    mp->trigger_timing_info.missed_triggers = triggerInfo[0].missedTriggers;
    mp->trigger_timing_info.prev_trigger_time = triggerInfo[0].timeStampCounter; 

    return status; 
}



/**
 * Stop picoscope data acquisition.
 * 
 * @param handle int16_t The handle of picoscope.
 * @return       PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
PICO_STATUS stop_capturing(int16_t handle) {
    PICO_STATUS status;
    epicsMutexLock(epics_ps6000a_call_mutex);
    status = ps6000aStop(handle);
    epicsMutexUnlock(epics_ps6000a_call_mutex);
    if (status != PICO_OK) {
        log_error("wait_for_capture_completion ps6000aStop", status, __FILE__, __LINE__);
    }
    return status;
}

void free_subwaveforms(struct PS6000AModule* mp, uint16_t subwaveform_num){
    for (size_t channel_index = 0; channel_index < NUM_CHANNELS; channel_index++)
    {
        if (mp->streamWaveformBuffers[channel_index])
        {
            for (uint16_t buffer_index = 0; buffer_index < subwaveform_num; buffer_index++)
            {
                free(mp->streamWaveformBuffers[channel_index][buffer_index]);
            }
            free(mp->streamWaveformBuffers[channel_index]);
        }
    }
}

inline bool do_Blocking(struct PS6000AModule* mp){
    return mp->subwaveform_num == 0;
}

/**
 * Thread function to handle the data acquisition and process data to epics PV.
 * 
 * @param arg Void Pointer Passed in from EPICS thread create, a PS6000A Module pointer.
 * @return    PICO_STATUS Returns PICO_OK (0) on success, or a non-zero error code on failure.
 */
void
acquisition_thread_function(void *arg) {
    PS6000AModule* mp = malloc(sizeof(PS6000AModule));
    *mp = *(struct PS6000AModule *)arg;
    while (1) // keep thread alive
    {
        epicsEventWait((epicsEventId)mp->acquisitionStartEvent);
        *mp = *(struct PS6000AModule *)arg;
        epicsMutexLock(mp->epics_acquisition_thread_mutex);
        mp->trigger_timing_info.prev_trigger_time = 0; // wipe previous trigger data  
        epicsThreadId id = epicsThreadGetIdSelf();
        printf("Start ID is %ld\n", id->tid);
        // Setup Picoscope
        printf("------------ Capture Configurations -----------------\n");
        printf("Resolution %d\n", mp->resolution);
        printf("Num samples %ld\n", mp->sample_config.num_samples);
        printf("timebase %d\n", mp->sample_config.timebase_configs.timebase);
        printf("pRecordUpdateWaveform aft %p\n", mp->pRecordUpdateWaveform[0]);

        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            if (get_channel_status(mp->channel_configs[i].channel, mp->channel_status) && mp->pRecordUpdateWaveform[i]) {
                printf("Updating waveform for Channel: %d, Coupling: %d, Range: %d, Analog Offest: %.10f, Bandwidth: %d\n",
                 mp->channel_configs[i].channel,
                 mp->channel_configs[i].coupling,
                 mp->channel_configs[i].range,
                 mp->channel_configs[i].analog_offset,
                 mp->channel_configs[i].bandwidth);
            }
        }
        printf("Num divisions %d\n", mp->sample_config.timebase_configs.num_divisions);
        printf("Time per divisions %f\n", mp->sample_config.timebase_configs.time_per_division);
        printf("Time per divisions unit %d\n", mp->sample_config.timebase_configs.time_per_division_unit);
        printf("Sample Interval (secs) %.10f\n", mp->sample_config.timebase_configs.sample_interval_secs);
        PICO_STATUS status = setup_picoscope(mp);
        if (status != 0) {
            log_error("Error configuring picoscope for data capture.", status, __FILE__, __LINE__);
            epicsMutexLock(mp->epics_acquisition_flag_mutex);
            *mp->dataAcquisitionFlag = 0;
            epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
            epicsMutexUnlock(mp->epics_acquisition_thread_mutex);
            // free(mp);
            continue;
        }
        uint16_t subwaveform_num = mp->subwaveform_num;
        printf("subwaveform_num %d, subwaveform_samples_num %ld\n\n", subwaveform_num, mp->sample_config.subwaveform_samples_num);
        printf("------------ Capture Configurations OVER--------------\n");

        while (*mp->dataAcquisitionFlag == 1) {
            double time_indisposed_ms = 0;
            if (do_Blocking(mp)){
                *mp->sample_collected = mp->sample_config.num_samples;
                status = run_block_capture(mp, &time_indisposed_ms);
                if (status != PICO_OK) {
                    printf("Error capturing block data.");
                    break;
                }
                // Process the UPDATE_WAVEFORM subroutine to update waveform
                for (size_t i = 0; i < NUM_CHANNELS; i++) {
                    if (get_channel_status(mp->channel_configs[i].channel, mp->channel_status) && mp->pRecordUpdateWaveform[i]) {
                        dbProcess((struct dbCommon *)mp->pRecordUpdateWaveform[i]);
                    }
                }

            }else{
                set_data_buffer(mp);
                status = run_stream_capture(mp);
                if (status != PICO_OK) {
                    printf("Error capturing stream data.\n");
                    break;
                }
            }

            dbProcess((dbCommon*) mp->pTriggerFrequency);
            dbProcess((dbCommon*) mp->pTriggersMissed); 
        }
        free_subwaveforms(mp, subwaveform_num);

        stop_capturing(mp->handle);
        printf("Cleanup ID is %ld\n", id->tid);
        epicsMutexLock(mp->epics_acquisition_flag_mutex);
        *mp->dataAcquisitionFlag = 0;
        epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
        epicsMutexUnlock(mp->epics_acquisition_thread_mutex);

    }
        free(mp);
}

 /**********************************************
 *  Module related functions 
 ***********************************************/
struct PS6000AModule*
PS6000AGetModule(char* serial_num){
    for (size_t i = 0; i < MAX_PICO; i++) {
        if (PS6000AModuleList[i] != NULL && strcmp(serial_num, PS6000AModuleList[i]->serial_num) == 0) {
            return PS6000AModuleList[i];
        }
    }
    // log_error("PS6000AGetModule. Device does not exist.", PICO_NOT_FOUND, __FILE__, __LINE__);
    return NULL;
}

static int init_module(char* serial_num) { 

	PS6000AModule *mp = PS6000AGetModule(serial_num);
    mp->triggerReadyEvent = epicsEventCreate(0);
    mp->acquisitionStartEvent = epicsEventCreate(0);
    for (size_t i = 0; i < NUM_CHANNELS; i++){
        mp->channelStreamingFinishedEvents[i] = epicsEventCreate(0);
    }
    mp->dataAcquisitionFlag = calloc(1 ,sizeof(int8_t));
    mp->sample_collected = calloc(1 ,sizeof(uint64_t));
    mp->epics_acquisition_restart_mutex = epicsMutexCreate();
    mp->epics_acquisition_flag_mutex = epicsMutexCreate();
    mp->epics_acquisition_thread_mutex = epicsMutexCreate();
    if (!(mp->epics_acquisition_flag_mutex = epicsMutexCreate()) ||
        !(mp->epics_acquisition_thread_mutex = epicsMutexCreate()) ||
        !(mp->epics_acquisition_restart_mutex = epicsMutexCreate())) {
        free(mp);
        log_error("Mutex creation failed\n", -1, __FILE__, __LINE__);
        return -1;
    }
    mp->subwaveform_num = 0;
    // Create capture thread
    mp->acquisition_thread_function = epicsThreadCreate("captureThread", epicsThreadPriorityMedium,
                                                     0, (EPICSTHREADFUNC)acquisition_thread_function, mp);
    if (!mp->acquisition_thread_function) {
        log_error("Thread creation failed\n", -1, __FILE__, __LINE__);
        epicsMutexLock(mp->epics_acquisition_flag_mutex);
        *mp->dataAcquisitionFlag = 0;
        epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
        return -1;
    }

    return 0;
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
    
    for (size_t i = 0; i < MAX_PICO; i++) {
        if (PS6000AModuleList[i] == NULL) {
            PS6000AModuleList[i] = mp;
        }
    }   

    if (init_module(serial_num) < 0 ){ 
        free(mp->serial_num);
        free(mp);
        log_error("PS6000ACreateModule", PICO_NO_APPS_AVAILABLE, __FILE__, __LINE__);
        return NULL; 
    }

    return mp; 
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
static const iocshArg * setPS6000AArgs[1] = {&setPS6000AArg0};
static iocshFuncDef PS6000ASetupDef = {"PS6000ASetup", 1, &setPS6000AArgs[0]};

void registerPS6000A(void)
{
	iocshRegister( &PS6000ASetupDef, PS6000ASetupCB);
}

epicsExportRegistrar(registerPS6000A);

void print_time(char* function_name){
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL); 
    tm_info = localtime(&tv.tv_sec);
    printf("%-45s: %04d-%02d-%02d %02d:%02d:%02d.%06ld\n",
        function_name,
        tm_info->tm_year + 1900,
        tm_info->tm_mon + 1,    
        tm_info->tm_mday,       
        tm_info->tm_hour,       
        tm_info->tm_min,        
        tm_info->tm_sec,        
        tv.tv_usec);
}