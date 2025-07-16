/*
 * ---------------------------------------------------------------------
 * File:
 *     drvPicoscope.h
 * Description:
 *     Driver interface for Picoscope PS6000A. Includes internal state 
 *     struct and API for EPICS integration.
 * ---------------------------------------------------------------------
 * Copyright (c) 2025 Canadian Light Source Inc.
 *
 * This file is part of DRIVER_Picoscope6000ESeries.
 *
 * DRIVER_Picoscope6000ESeries is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DRIVER_Picoscope6000ESeries is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef DRV_PICOSCOPE
#define DRV_PICOSCOPE

#include "picoscopeConfig.h"
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsThread.h>


typedef struct PS6000AModule 
{
    char* serial_num;
    int16_t handle;
    int8_t status;
    int16_t resolution; 


    int8_t *dataAcquisitionFlag;
    struct waveformRecord* pWaveformStartPtr;
    struct waveformRecord* pWaveformStopPtr;
    struct waveformRecord* pRecordUpdateWaveform[NUM_CHANNELS];
    struct waveformRecord* pLog;
    uint16_t subwaveform_num;
    uint64_t waveform_size;

    epicsEventId triggerReadyEvent;
    epicsEventId acquisitionStartEvent;
    epicsEventId channelStreamingFinishedEvents[NUM_CHANNELS];
    epicsMutexId epics_acquisition_flag_mutex;
    epicsMutexId epics_acquisition_thread_mutex;
    epicsMutexId epics_acquisition_restart_mutex;
    epicsThreadId acquisition_thread_function;
    epicsThreadId channel_streaming_thread_function[4];
    int16_t* waveform[NUM_CHANNELS];
    int16_t** streamWaveformBuffers[NUM_CHANNELS];


    struct SampleConfigs sample_config;
    struct ChannelConfigs channel_configs[NUM_CHANNELS];
    struct TriggerConfigs trigger_config;

    EnabledChannelFlags channel_status; 

    // Stored PVs for processing at specific time 
    struct aiRecord* pTriggerThresholdFbk[4];
    struct aoRecord* pTriggerThreshold[2]; 
    struct aoRecord* pAnalogOffestRecords[NUM_CHANNELS];

    struct aiRecord* pTriggerFrequency; 
    struct aiRecord* pTriggersMissed; 

    struct mbboRecord* pTriggerDirection;
    struct mbbiRecord* pTriggerDirectionFbk;
    struct mbbiRecord* pTriggerType;
    struct mbbiRecord* pTriggerChannelFbk;
    struct mbbiRecord* pTriggerModeFbk;
    struct mbboRecord* pTimePerDivision;
    struct mbbiRecord* pTimePerDivisionFbk;
    uint64_t *sample_collected;

    struct TriggerTimingInfo trigger_timing_info; 

} PS6000AModule;

void print_time(char* function_name);

PS6000AModule* PS6000ACreateModule(char* serial_num);

PS6000AModule* PS6000AGetModule(char* serial_num);

uint32_t get_device_info(int8_t** device_info, int16_t handle);

uint32_t open_picoscope(struct PS6000AModule* mp, int16_t* handle); 

uint32_t ping_picoscope(struct PS6000AModule* mp);

uint32_t set_resolution(int16_t resolution, int16_t handle);

uint32_t get_resolution(int16_t* resolution, int16_t handle);

uint32_t close_picoscope(struct PS6000AModule* mp);

uint32_t set_channel_on(struct ChannelConfigs channel, int16_t handle, EnabledChannelFlags* channel_status);

uint32_t set_channel_off(int channel, int16_t handle, EnabledChannelFlags* channel_status);

uint32_t get_channel_status(int16_t channel, EnabledChannelFlags channel_status);

uint32_t calculate_scaled_value(double volts, int16_t voltage_range, int16_t* scaled_value, int16_t handle);

uint32_t get_adc_limits(int16_t* min_val, int16_t* max_val, int16_t handle);

uint32_t get_valid_timebase_configs(
    struct PS6000AModule* mp,
    double* sample_interval,
    uint32_t* timebase, 
    double* sample_rate
    );  

uint32_t get_analog_offset_limits(
    struct ChannelConfigs channel, 
    int16_t handle,
    double* max_analog_offset,
    double* min_analog_offset
    );


// uint32_t setup_picoscope(
//     struct PS6000AModule* mp
//     );

// uint32_t interrupt_block_capture();

uint32_t run_block_capture(
    struct PS6000AModule* mp,
    double* time_indisposed_ms
    );

uint32_t get_analogue_offset_limits(
    int16_t range, 
    int16_t coupling, 
    double* max_analogue_offset,
    double* min_analogue_offset
    );

uint32_t validate_sample_interval(
    double requested_time_interval, 
    int16_t handle, 
    EnabledChannelFlags channel_status,
    uint32_t* timebase, 
    double* available_time_interval
    );

uint32_t stop_capturing(int16_t handle);


#endif
