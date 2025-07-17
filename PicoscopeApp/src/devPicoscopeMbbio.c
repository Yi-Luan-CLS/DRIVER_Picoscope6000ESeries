/*
 * ---------------------------------------------------------------------
 * File:
 *     devPicoscopeMbbio.c
 * Description:
 *     Device support for EPICS mbbo and mbbi records for Picoscope 
 *     PS6000A module. Provides control and status interfaces for 
 *     resolution, timebase, trigger configuration, channel settings, 
 *     and acquisition parameters using multi-bit binary records.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsExport.h>
#include <errlog.h>

#include <mbbiRecord.h>
#include <mbboRecord.h>

#include "devPicoscopeCommon.h"

enum ioType 
{
    UNKNOWN_IOTYPE,
    SET_RESOLUTION,
    GET_RESOLUTION,
    SET_DOWN_SAMPLE_RATIO_MODE,
    GET_DOWN_SAMPLE_RATIO_MODE,
    SET_TRIGGER_DIRECTION,
    GET_TRIGGER_DIRECTION,
    SET_TRIGGER_TYPE, 
    GET_TRIGGER_TYPE, 
    SET_TIME_PER_DIVISION_UNIT, 
    GET_TIME_PER_DIVISION_UNIT, 
    SET_TIME_PER_DIVISION, 
    GET_TIME_PER_DIVISION,
    SET_TRIGGER_CHANNEL,
    GET_TRIGGER_CHANNEL, 
    GET_TRIGGER_MODE,
    SET_COUPLING,
    GET_COUPLING,
    SET_RANGE, 
    GET_RANGE,
    SET_BANDWIDTH, 
    GET_BANDWIDTH,
};

enum ioFlag
{
    isOutput = 0,
    isInput = 1
};


struct mbbioType
{
	char *label;
	enum ioFlag flag;
	enum ioType ioType;
    char *cmdp;
} MbbioType[] =
{
		/* string			              in/out		 function		              command */
        {"set_resolution",                isOutput,     SET_RESOLUTION,                 ""},
        {"get_resolution",                isInput,      GET_RESOLUTION,                 ""},
        {"set_down_sampling_ratio_mode",  isOutput,     SET_DOWN_SAMPLE_RATIO_MODE,     ""},
        {"get_down_sampling_ratio_mode",  isInput,      GET_DOWN_SAMPLE_RATIO_MODE,     ""},
        {"set_trigger_type",              isOutput,     SET_TRIGGER_TYPE,               ""},
        {"get_trigger_type",              isInput,      GET_TRIGGER_TYPE,               ""},
        {"set_trigger_direction",         isOutput,     SET_TRIGGER_DIRECTION,          ""},
        {"get_trigger_direction",         isInput,      GET_TRIGGER_DIRECTION,          ""}, 
        {"set_time_per_division_unit",    isOutput,     SET_TIME_PER_DIVISION_UNIT,     ""},
        {"get_time_per_division_unit",    isInput,      GET_TIME_PER_DIVISION_UNIT,     ""},
        {"set_time_per_division",         isOutput,     SET_TIME_PER_DIVISION,          ""},
        {"get_time_per_division",         isInput,      GET_TIME_PER_DIVISION,          ""},
        {"set_trigger_channel",           isOutput,     SET_TRIGGER_CHANNEL,            ""},       
        {"get_trigger_channel",           isInput,      GET_TRIGGER_CHANNEL,            ""},       
        {"get_trigger_mode",              isInput,      GET_TRIGGER_MODE,               ""},
        {"set_coupling",                  isOutput,     SET_COUPLING,                   ""},
        {"get_coupling",                  isInput,      GET_COUPLING,                   ""},
        {"set_range",                     isOutput,     SET_RANGE,                      ""}, 
        {"get_range",                     isInput,      GET_RANGE,                      ""},
        {"set_bandwidth",                 isOutput,     SET_BANDWIDTH,                  ""}, 
        {"get_bandwidth",                 isInput,      GET_BANDWIDTH,                  ""}, 

};


#define MBBIO_TYPE_SIZE    (sizeof (MbbioType) / sizeof (struct mbbioType))

struct PicoscopeMbbioData
    {   
        char serial_num[10];
        enum ioType ioType;
        char *cmdPrefix;
        char paramLabel[32];
        int paramValid;
        struct PS6000AModule* mp;
    };

static enum ioType findMbbioType(enum ioFlag ioFlag, char *param, char **cmdString)
{
	unsigned int i;

	for (i = 0; i < MBBIO_TYPE_SIZE; i ++){
		if (strncmp(param, MbbioType[i].label,strlen(MbbioType[i].label)) == 0  &&
				MbbioType[i].flag == ioFlag)
		{
			*cmdString = MbbioType[i].cmdp;
			return MbbioType[i].ioType;
		}
	}

	return UNKNOWN_IOTYPE;
}


/****************************************************************************************
 * Multi-Bit Binary Output Records - mbbo
 ****************************************************************************************/

typedef long (*DEVSUPFUN_MBBO)(struct mbboRecord*);

static long init_record_mbbo(struct mbboRecord *pmbbo);
static long write_mbbo (struct mbboRecord *pmbbo);

struct
{
    long         number;
    DEVSUPFUN_MBBO report;
    DEVSUPFUN_MBBO init;
    DEVSUPFUN_MBBO init_record;
    DEVSUPFUN_MBBO get_ioint_info;
    DEVSUPFUN_MBBO write_lout;
} devPicoscopeMbbo =
    {
        6,
        NULL,
        NULL,
        init_record_mbbo,
        NULL,
        write_mbbo, 
    };

epicsExportAddress(dset, devPicoscopeMbbo);


static long init_record_mbbo (struct mbboRecord *pmbbo)
{ 
    char* record_name; 
    int channel_index; 

    struct instio  *pinst;
    struct PicoscopeMbbioData *vdp;

    if (pmbbo->out.type != INST_IO)
    {
        errlogPrintf("%s: INP field type should be INST_IO\n", pmbbo->name);
        return(S_db_badField);
    }
    pmbbo->dpvt = calloc(sizeof(struct PicoscopeMbbioData), 1);
    if (pmbbo->dpvt == (void *)0)
    {
        errlogPrintf("%s: Failed to allocated memory\n", pmbbo->name);
        return -1;
    }
  
    pinst = &(pmbbo->out.value.instio);
    vdp = (struct PicoscopeMbbioData *)pmbbo->dpvt;

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

    vdp->ioType = findMbbioType(isOutput, vdp->paramLabel, &(vdp->cmdPrefix));
    if (vdp->ioType == UNKNOWN_IOTYPE)
    {
        errlogPrintf("%s: Invalid type: \"%s\"\n", pmbbo->name, vdp->paramLabel);
        return(S_db_badField);
    }

    pmbbo->udf = FALSE;

    switch (vdp->ioType)
    {      
        case SET_COUPLING:    
            record_name = pmbbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     
            
            vdp->mp->channel_configs[channel_index].coupling = (int)pmbbo->rval;
            break;

        case SET_RANGE: 
            record_name = pmbbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     
            
            vdp->mp->channel_configs[channel_index].range = (int)pmbbo->rval;
            break;
        
        case SET_BANDWIDTH: 
            record_name = pmbbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     

            vdp->mp->channel_configs[channel_index].bandwidth = (int)pmbbo->rval;
            break;

        case SET_TIME_PER_DIVISION: 
            vdp->mp->pTimePerDivision = pmbbo; 
            vdp->mp->sample_config.timebase_configs.time_per_division = (int) pmbbo->rval; 
            break; 

        case SET_TIME_PER_DIVISION_UNIT: 
            vdp->mp->sample_config.timebase_configs.time_per_division_unit = (int) pmbbo->val; 
            break;

        case SET_DOWN_SAMPLE_RATIO_MODE: 
            vdp->mp->sample_config.down_sample_ratio_mode = (int)pmbbo->rval;
            break; 

        case SET_TRIGGER_CHANNEL:
           vdp->mp->trigger_config.channel  = (enum Channel) pmbbo->rval;
           break;

        case SET_TRIGGER_TYPE: 
            vdp->mp->trigger_config.triggerType = (int) pmbbo->rval; 
            break; 
        
        case SET_TRIGGER_DIRECTION:
            vdp->mp->trigger_config.thresholdDirection = (enum ThresholdDirection) pmbbo->val;
            vdp->mp->pTriggerDirection = pmbbo;
            break;


        default:
            return 0;
    }

    return 0; 
}


static long
write_mbbo (struct mbboRecord *pmbbo)
{   
    char* record_name; 
    int channel_index; 
    uint32_t timebase = 0; 
    double sample_interval, sample_rate = 0; 
    char log_message[LOG_MESSAGE_LENGTH] = {0};
    uint32_t result = 0; 

    struct PicoscopeMbbioData *vdp = (struct PicoscopeMbbioData *)pmbbo->dpvt;
    int returnState = 0; 

    switch (vdp->ioType)
    {        

        case SET_RESOLUTION: 
            int16_t resolution = (int)pmbbo->rval; 
            result = set_resolution(vdp->mp, resolution); 
            if (result !=0) {
                update_log_pvs(vdp->mp, "Failed to set resolution", result);
                break;
            } 
        
            vdp->mp->resolution = resolution; 
            // Sampling resolution affects max and min trigger thresholds, process threshold PVs
            // to ensure thresholds are still within the limits at the new resolution.  
            for(size_t i = 0; i < sizeof(vdp->mp->pTriggerThreshold)/ sizeof(vdp->mp->pTriggerThreshold[0]); i++){ 
                dbProcess((struct dbCommon *) vdp->mp->pTriggerThreshold[i]);            
            }
        
            update_log_pvs(vdp->mp, NULL, result);
            break;  

        case SET_COUPLING:    
            record_name = pmbbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 

            // Coupling affects the range of valid analog offset values, processing this PV will ensure 
            // the offset for the channel is valid with the new coupling. 
            dbProcess((struct dbCommon *)vdp->mp->pAnalogOffestRecords[channel_index]);     
    
            int16_t previous_coupling = vdp->mp->channel_configs[channel_index].coupling; 
            vdp->mp->channel_configs[channel_index].coupling = (int)pmbbo->rval;

            uint32_t channel_status = get_channel_status(vdp->mp->channel_configs[channel_index].channel, vdp->mp->channel_status); 
            if (channel_status == 1) {
                result = set_channel_on(
                    vdp->mp,
                    vdp->mp->channel_configs[channel_index], 
                    &vdp->mp->channel_status
                );                
                // If channel is not succesfully set on, return to previous value 
                if (result != 0) {
                    snprintf(log_message, sizeof(log_message), "Error setting coupling to %d", (int)pmbbo->rval);
                    vdp->mp->channel_configs[channel_index].coupling = previous_coupling;
                }
            }
            update_log_pvs(vdp->mp, log_message[0] ? log_message : NULL, result);
            break;

        case SET_RANGE:
            record_name = pmbbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     

            int16_t previous_range = vdp->mp->channel_configs[channel_index].range; 

            vdp->mp->channel_configs[channel_index].range = (int)pmbbo->rval;
        
            // Voltage range affects the range of valid analog offset values, processing this PV will ensure 
            // the offset for the channel is valid with the new range. 
            dbProcess((struct dbCommon *)vdp->mp->pAnalogOffestRecords[channel_index]);     

            channel_status = get_channel_status(vdp->mp->channel_configs[channel_index].channel, vdp->mp->channel_status); 
            if (channel_status == 1){
                result = set_channel_on(
                    vdp->mp,
                    vdp->mp->channel_configs[channel_index], 
                    &vdp->mp->channel_status
                );     
                // If channel is not succesfully set on, return to previous value 
                if (result != 0) {
                    snprintf(log_message, sizeof(log_message), "Error setting voltage range to %d", (int)pmbbo->rval);
                    vdp->mp->channel_configs[channel_index].range = previous_range;
                }
            }

            // Process trigger thresholds to ensure triggers are set between ±range. 
            for(size_t i = 0; i < sizeof(vdp->mp->pTriggerThreshold)/ sizeof(vdp->mp->pTriggerThreshold[0]); i++){ 
                dbProcess((struct dbCommon *) vdp->mp->pTriggerThreshold[i]);            
            }
            
            update_log_pvs(vdp->mp, log_message[0] ? log_message : NULL, result);
            break;

        
        case SET_BANDWIDTH: 
            record_name = pmbbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     
            
            int16_t previous_bandwidth = vdp->mp->channel_configs[channel_index].bandwidth;

            vdp->mp->channel_configs[channel_index].bandwidth = (int)pmbbo->rval;

            channel_status = get_channel_status(vdp->mp->channel_configs[channel_index].channel, vdp->mp->channel_status); 
            if (channel_status == 1) {
                result = set_channel_on(
                    vdp->mp, 
                    vdp->mp->channel_configs[channel_index], 
                    &vdp->mp->channel_status
                );                
                // If channel is not succesfully set on, return to previous value 
                if (result != 0) {
                    snprintf(log_message, sizeof(log_message), "Error setting bandwidth to %d", (int)pmbbo->rval);
                    vdp->mp->channel_configs[channel_index].bandwidth = previous_bandwidth;
                }
            }

            update_log_pvs(vdp->mp, log_message[0] ? log_message : NULL, result);
            break;

        case SET_TIME_PER_DIVISION: 
            double previous_time_per_division = vdp->mp->sample_config.timebase_configs.time_per_division; 
            vdp->mp->sample_config.timebase_configs.time_per_division = (int) pmbbo->rval; 

            result = get_valid_timebase_configs(
                vdp->mp,
                &sample_interval, 
                &timebase, 
                &sample_rate
            ); 
            
            if (result != 0) {
                snprintf(log_message, sizeof(log_message), "Error setting time per division to %d", (int)pmbbo->rval);
                vdp->mp->sample_config.timebase_configs.time_per_division = previous_time_per_division;  
            } else {
                vdp->mp->sample_config.timebase_configs.sample_interval_secs = sample_interval;
                vdp->mp->sample_config.timebase_configs.timebase = timebase;
                vdp->mp->sample_config.timebase_configs.sample_rate = sample_rate;  
            }

            update_log_pvs(vdp->mp, log_message[0] ? log_message : NULL, result);
            break; 

        case SET_TIME_PER_DIVISION_UNIT:
            if (pmbbo->val == s_per_div) { 
                // If unit is seconds, limit to 10 secs per div and update PVs. 
                MultiBitBinaryEnums s_per_div_options = {0};
                s_per_div_options.frst = ""; s_per_div_options.frvl = 0; 
                s_per_div_options.fvst = ""; s_per_div_options.fvvl = 0; 
                s_per_div_options.sxst = ""; s_per_div_options.sxvl = 0; 
                s_per_div_options.svst = ""; s_per_div_options.svvl = 0; 
                s_per_div_options.eist = ""; s_per_div_options.eivl = 0; 
                s_per_div_options.nist = ""; s_per_div_options.nivl = 0; 
                s_per_div_options.test = ""; s_per_div_options.tevl = 0;
                s_per_div_options.elst = ""; s_per_div_options.elvl = 0;              
    
                update_enum_options(
                    vdp->mp->pTimePerDivision,
                    vdp->mp->pTimePerDivisionFbk, 
                    s_per_div_options
                );

                // If the time per division is no longer valid, update PVs to valid option. 
                if (vdp->mp->sample_config.timebase_configs.time_per_division > 10){
                    vdp->mp->sample_config.timebase_configs.time_per_division = 1; 
                    dbProcess((struct dbCommon *)vdp->mp->pTimePerDivisionFbk); 
                }

            } else if (pmbbo->oraw == s_per_div) {  
                // If last unit was seconds, update time per division options to full set of options. 
                MultiBitBinaryEnums under_s_per_div_options = {0};
                under_s_per_div_options.frst = "20";   under_s_per_div_options.frvl = 20; 
                under_s_per_div_options.fvst = "50";   under_s_per_div_options.fvvl = 50; 
                under_s_per_div_options.sxst = "100";  under_s_per_div_options.sxvl = 100; 
                under_s_per_div_options.svst = "200";  under_s_per_div_options.svvl = 200; 
                under_s_per_div_options.eist = "500";  under_s_per_div_options.eivl = 500; 
                under_s_per_div_options.nist = "1000"; under_s_per_div_options.nivl = 1000; 
                under_s_per_div_options.test = "2000"; under_s_per_div_options.tevl = 2000;
                under_s_per_div_options.elst = "5000"; under_s_per_div_options.elvl = 5000;

                update_enum_options(
                    vdp->mp->pTimePerDivision,
                    vdp->mp->pTimePerDivisionFbk, 
                    under_s_per_div_options
                );

            }
            int16_t previous_time_per_division_unit = vdp->mp->sample_config.timebase_configs.time_per_division_unit;
            vdp->mp->sample_config.timebase_configs.time_per_division_unit = (int) pmbbo->val; 

            result = get_valid_timebase_configs(
                vdp->mp,
                &sample_interval, 
                &timebase, 
                &sample_rate
            ); 

            if (result != 0) {
                snprintf(log_message, sizeof(log_message), "Error setting time per division unit to %d", (int)pmbbo->rval);
                vdp->mp->sample_config.timebase_configs.time_per_division_unit = previous_time_per_division_unit; 
            } else { 
                vdp->mp->sample_config.timebase_configs.sample_interval_secs = sample_interval;
                vdp->mp->sample_config.timebase_configs.timebase = timebase;
                vdp->mp->sample_config.timebase_configs.sample_rate = sample_rate;  
            }

            update_log_pvs(vdp->mp, log_message[0] ? log_message : NULL, result);
            break; 


        case SET_DOWN_SAMPLE_RATIO_MODE: 
            vdp->mp->sample_config.down_sample_ratio_mode = (int)pmbbo->rval;
            break;
        
        case SET_TRIGGER_CHANNEL:
            vdp->mp->trigger_config.channel = (enum Channel) pmbbo->rval;
            if (vdp->mp->trigger_config.channel == TRIGGER_AUX)
            {    
                vdp->mp->trigger_config.triggerType = SIMPLE_EDGE;
                vdp->mp->trigger_config.thresholdMode = LEVEL; 
                vdp->mp->trigger_config.thresholdLower = 0; 
                vdp->mp->trigger_config.thresholdLowerHysteresis = 0; 
                vdp->mp->trigger_config.thresholdUpper = 0; 
                vdp->mp->trigger_config.thresholdUpperHysteresis = 0; 
                vdp->mp->trigger_config.thresholdDirection = NONE; 
                
            }
            else if (vdp->mp->trigger_config.channel == NO_CHANNEL) {
                vdp->mp->trigger_config.triggerType = NO_TRIGGER;
                vdp->mp->trigger_config.thresholdMode = LEVEL; 
                vdp->mp->trigger_config.thresholdLower = 0; 
                vdp->mp->trigger_config.thresholdLowerHysteresis = 0; 
                vdp->mp->trigger_config.thresholdUpper = 0; 
                vdp->mp->trigger_config.thresholdUpperHysteresis = 0; 
                vdp->mp->trigger_config.thresholdDirection = NONE; 
            } else {
                if (vdp->mp->trigger_config.triggerType == NO_TRIGGER) {
                    vdp->mp->trigger_config.triggerType = SIMPLE_EDGE;
                }
            }

            // Update all trigger info to valid options for the trigger channel 
            dbProcess((struct dbCommon *)vdp->mp->pTriggerType); 
            dbProcess((struct dbCommon *)vdp->mp->pTriggerDirectionFbk);
            dbProcess((struct dbCommon *)vdp->mp->pTriggerModeFbk);
            for (size_t i = 0; i < sizeof(vdp->mp->pTriggerThresholdFbk)/sizeof(vdp->mp->pTriggerThresholdFbk[0]); i++)
            {
                dbProcess((struct dbCommon *)vdp->mp->pTriggerThresholdFbk[i]);
            }
            break;

        case SET_TRIGGER_TYPE:             
            vdp->mp->trigger_config.triggerType = (int)pmbbo->val; 
            
            // Update related configurations based on trigger type selected. Including changes
            // to trigger direction mbbo/mbbi options and trigger_config members.
            switch (vdp->mp->trigger_config.triggerType)
            {   
                case NO_TRIGGER: 
                    vdp->mp->trigger_config.channel = NO_CHANNEL; 
                    vdp->mp->trigger_config.thresholdMode = LEVEL;             
                    vdp->mp->trigger_config.thresholdLower = 0; 
                    vdp->mp->trigger_config.thresholdUpper = 0; 
                    vdp->mp->trigger_config.thresholdLowerHysteresis = 0; 
                    vdp->mp->trigger_config.thresholdUpperHysteresis = 0; 
                    vdp->mp->trigger_config.thresholdDirection = NONE; 

                    MultiBitBinaryEnums no_trigger_options = {0};
                    no_trigger_options.zrst = "NONE";   no_trigger_options.zrvl = NONE;
                    no_trigger_options.onst = "";       no_trigger_options.onvl = 0;
                    no_trigger_options.twst = "";       no_trigger_options.twvl = 0;
                    
                    update_enum_options(
                        vdp->mp->pTriggerDirection,
                        vdp->mp->pTriggerDirectionFbk, 
                        no_trigger_options
                    );
                    break; 
                
                case SIMPLE_EDGE:
                    if (vdp->mp->trigger_config.channel == NO_CHANNEL) {
                        vdp->mp->trigger_config.channel = TRIGGER_AUX;
                        vdp->mp->trigger_config.thresholdUpper = 0;
                        vdp->mp->trigger_config.thresholdUpperHysteresis = 0;
                    } 
                    vdp->mp->trigger_config.thresholdMode = LEVEL;         
                    vdp->mp->trigger_config.thresholdLower = 0;
                    
                    MultiBitBinaryEnums simple_edge_options = {0};
                    simple_edge_options.zrst = "RISING";  simple_edge_options.zrvl = RISING; 
                    simple_edge_options.onst = "FALLING"; simple_edge_options.onvl = FALLING; 
                    simple_edge_options.twst = "";        simple_edge_options.twvl = 0;
                    
                    update_enum_options(
                        vdp->mp->pTriggerDirection,
                        vdp->mp->pTriggerDirectionFbk, 
                        simple_edge_options
                    );
                    break; 

                case WINDOW:
                    if (vdp->mp->trigger_config.channel == NO_CHANNEL || vdp->mp->trigger_config.channel == TRIGGER_AUX) {
                        vdp->mp->trigger_config.channel = CHANNEL_A;
                    } 
                    vdp->mp->trigger_config.thresholdMode = WINDOW_MODE;         
                    
                    MultiBitBinaryEnums window_options = {0};
                    window_options.zrst = "ENTER";          window_options.zrvl = ENTER; 
                    window_options.onst = "EXIT";           window_options.onvl = EXIT; 
                    window_options.twst = "ENTER OR EXIT";  window_options.twvl = ENTER_OR_EXIT;
                   
                    update_enum_options(
                        vdp->mp->pTriggerDirection,
                        vdp->mp->pTriggerDirectionFbk, 
                        window_options
                    );
                    break; 
                
                case ADVANCED_EDGE: 
                    if (vdp->mp->trigger_config.channel == NO_CHANNEL || vdp->mp->trigger_config.channel == TRIGGER_AUX) {
                        vdp->mp->trigger_config.channel = CHANNEL_A;
                    } 
                    vdp->mp->trigger_config.thresholdMode = LEVEL;         
                    vdp->mp->trigger_config.thresholdLower = 0;
                    vdp->mp->trigger_config.thresholdLowerHysteresis = 0;   

                    MultiBitBinaryEnums advanced_edge_options = {0};
                    advanced_edge_options.zrst = "RISING";             advanced_edge_options.zrvl = RISING;
                    advanced_edge_options.onst = "FALLING";            advanced_edge_options.onvl = FALLING; 
                    advanced_edge_options.twst = "RISING OR FALLING";  advanced_edge_options.twvl = RISING_OR_FALLING;
                   
                    update_enum_options(
                        vdp->mp->pTriggerDirection,
                        vdp->mp->pTriggerDirectionFbk, 
                        advanced_edge_options
                    );
                    break;

                default: 
                    return -1; 
            }

            // Update all trigger info to valid options for the trigger type
            dbProcess((struct dbCommon *)vdp->mp->pTriggerChannelFbk);
            dbProcess((struct dbCommon *)vdp->mp->pTriggerDirectionFbk);
            dbProcess((struct dbCommon *)vdp->mp->pTriggerModeFbk);
            for (size_t i = 0; i < sizeof(vdp->mp->pTriggerThresholdFbk)/sizeof(vdp->mp->pTriggerThresholdFbk[0]); i++)
                {
                    dbProcess((struct dbCommon *)vdp->mp->pTriggerThresholdFbk[i]);
                }
            break; 

        case SET_TRIGGER_DIRECTION:
            
            if (vdp->mp->trigger_config.triggerType == NO_TRIGGER) {
                vdp->mp->trigger_config.thresholdDirection = NONE; 
            }
            else {
                vdp->mp->trigger_config.thresholdDirection = (enum ThresholdDirection) pmbbo->rval;
            }
            break;
        
        default:
            return 0;
    }


    if (returnState < 0)
    {
        if (recGblSetSevr(pmbbo, READ_ALARM, INVALID_ALARM)  &&  errVerbose
            &&  (pmbbo->stat != READ_ALARM  ||  pmbbo->sevr != INVALID_ALARM))
            {
                errlogPrintf("%s: Read Error\n", pmbbo->name);
            }
        return 2;
    }else{
        re_acquire_waveform(vdp->mp);
    }
    return 0;
}




/****************************************************************************************
 * Multi-Bit Binary Input Records - mbbi
 ****************************************************************************************/

typedef long (*DEVSUPFUN_MBBI)(struct mbbiRecord *);

static long init_record_mbbi(struct mbbiRecord *);
static long read_mbbi(struct mbbiRecord *);

struct
{
    long         number;
    DEVSUPFUN_MBBI report;
    DEVSUPFUN_MBBI init;
    DEVSUPFUN_MBBI init_record;
    DEVSUPFUN_MBBI get_ioint_info;
    DEVSUPFUN_MBBI read;
} devPicoscopeMbbi =
    {
        6,
        NULL,
        NULL,
        init_record_mbbi,
        NULL,
        read_mbbi,
    };

epicsExportAddress(dset, devPicoscopeMbbi);



static long init_record_mbbi(struct mbbiRecord * pmbbi)
{
    struct instio  *pinst;
    struct PicoscopeMbbioData *vdp;

    if (pmbbi->inp.type != INST_IO)
    {
        printf("%s: INP field type should be INST_IO\n", pmbbi->name);
        return(S_db_badField);
    }
    
    pmbbi->dpvt = calloc(sizeof(struct PicoscopeMbbioData), 1);
    
    if (pmbbi->dpvt == (void *)0)
    {
        printf("%s: Failed to allocated memory\n", pmbbi->name);
        return -1;
    }
    
    pinst = &(pmbbi->inp.value.instio);
    vdp = (struct PicoscopeMbbioData *)pmbbi->dpvt;

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

    vdp->ioType = findMbbioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));
    if (vdp->ioType == UNKNOWN_IOTYPE){
        printf("%s: Invalid type: \"@%s\"\n", pmbbi->name, vdp->paramLabel);
        return(S_db_badField);
    }
    
    pmbbi->udf = FALSE;

    switch (vdp->ioType) {

        case GET_RESOLUTION:
            int16_t resolution; 
            get_resolution(vdp->mp, &resolution);
            pmbbi->val = resolution;  
            break; 

        case GET_TRIGGER_CHANNEL:
            vdp->mp->pTriggerChannelFbk = pmbbi;
            break;

        case GET_TRIGGER_MODE:
            vdp->mp->pTriggerModeFbk = pmbbi;
            break;

        case GET_TRIGGER_TYPE: 
            vdp->mp->pTriggerType = pmbbi;
            break;  

        case GET_TRIGGER_DIRECTION:
            vdp->mp->pTriggerDirectionFbk = pmbbi; 
            break;  

        case GET_TIME_PER_DIVISION: 
            vdp->mp->pTimePerDivisionFbk = pmbbi; 
            break; 
        
        default: 
            return 0; 
    } 

    return 0;
}

static long read_mbbi(struct mbbiRecord *pmbbi){
    
    char* record_name; 
    int channel_index; 
    
    struct PicoscopeMbbioData *vdp = (struct PicoscopeMbbioData *)pmbbi->dpvt;

    switch (vdp->ioType)
    {
        case GET_RESOLUTION: 
            int16_t resolution;
            uint32_t result = get_resolution(vdp->mp, &resolution);
            if (result != 0) {
                update_log_pvs(vdp->mp, "Error getting device resolution.", result);
            } else {
                pmbbi->rval = resolution; 
                update_log_pvs(vdp->mp, NULL, result);
            } 
            break; 
        
        case GET_COUPLING: 
            record_name = pmbbi->name; 
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 

            pmbbi->rval = vdp->mp->channel_configs[channel_index].coupling;
            break; 

        case GET_RANGE: 
            record_name = pmbbi->name; 
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 

            pmbbi->rval = vdp->mp->channel_configs[channel_index].range; 
            break; 

        case GET_BANDWIDTH: 
            record_name = pmbbi->name; 
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 

            pmbbi->rval = vdp->mp->channel_configs[channel_index].bandwidth; 
            break; 

        case GET_TRIGGER_MODE:
            pmbbi->rval = vdp->mp->trigger_config.thresholdMode;
            break;

        case GET_TRIGGER_CHANNEL:
            pmbbi->rval = vdp->mp->trigger_config.channel;
            break;

        case GET_TIME_PER_DIVISION: 
            pmbbi->rval = vdp->mp->sample_config.timebase_configs.time_per_division; 
            break; 

        case GET_TIME_PER_DIVISION_UNIT: 
            pmbbi->rval = vdp->mp->sample_config.timebase_configs.time_per_division_unit; 
            break;

        case GET_DOWN_SAMPLE_RATIO_MODE: 
            pmbbi->rval = vdp->mp->sample_config.down_sample_ratio_mode; 
            break;
        
        case GET_TRIGGER_DIRECTION:
            pmbbi->rval = vdp->mp->trigger_config.thresholdDirection; 
            break;
        
        case GET_TRIGGER_TYPE: 
            pmbbi->rval = vdp->mp->trigger_config.triggerType; 
            break; 

        default:
            return 0;
    }

    return 0; 
}   
