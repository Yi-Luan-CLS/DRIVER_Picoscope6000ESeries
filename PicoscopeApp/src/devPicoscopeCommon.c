/*
 * ---------------------------------------------------------------------
 * Copyright 2025 Canadian Light Source, Inc. All rights reserved
 *     - see LICENSE.md for limitations on use.
 * 
 * Description:
 *     Common utility functions for EPICS device support of Picoscope 
 *     PS6000A modules. Includes parameter parsing, channel indexing,
 *     waveform reacquisition, logging, and enum field updates for
 *     bi/bo and mbbi/mbbo records.
 * ---------------------------------------------------------------------
*/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dbAccess.h>

#include "picoscopeConfig.h"
#include "devPicoscopeCommon.h"

/**
 * Converts parameters specified in a records INP/OUT field with the
 * format "@S:$(SERIAL_NUM) @L:${PARAM_NAME}". 
 * 
 * @param string The string in the above format. 
 *        paramName On exit, the parameter name associated with the PV. 
 *        serialNum On exit, the serial number of the device associated 
 *                  with the PV. 
 */ 
int convertPicoscopeParams(char *string, char *paramName, char *serialNum)
{       
    if (sscanf(string, "S:%s @L:%s", serialNum, paramName) != 2) {
        return -1;
    }
    return 0;
}

void re_acquire_waveform(struct PS6000AModule *mp){
    if (*mp->dataAcquisitionFlag!=1) {
        return;
    }
    epicsMutexLock(mp->epics_acquisition_restart_mutex);    // this is to make sure Stop and Start PV invoked in sequence.
    
    dbProcess((struct dbCommon *)mp->pWaveformStopPtr);

    // this is to make sure the capureting thread is actually stopped
    epicsMutexLock(mp->epics_acquisition_thread_mutex);
    epicsMutexUnlock(mp->epics_acquisition_thread_mutex);
    
    dbProcess((struct dbCommon *)mp->pWaveformStartPtr);
    
    epicsMutexUnlock(mp->epics_acquisition_restart_mutex);
}

/** 
 * Gets the channel from the record name formatted "OSCXXXX-XX:CH[A-B]:" and returns index of that channel 
 * from an array of ChannelConfigs. 
 * 
 * @param record_name PV name formated "OSCXXXX-XX:CH[A-B]:"
 *           channels Array of ChannelConfigs 
 * 
 * @returns Index of channel in the channels array if successful, otherwise returns -1 
 * */
inline int find_channel_index_from_record(const char* record_name, struct ChannelConfigs channel_configs[NUM_CHANNELS]) {
    char channel_str[4];
    if (sscanf(record_name, "%*[^:]:%3[^:]", channel_str) != 1){  // Extract the channel part, e.g., "CHA", "CHB", etc.
        return -1; 
    }

    enum Channel channel = NO_CHANNEL;
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        if (strcmp(channel_str, PV_TO_CHANNEL_MAP[i].name) == 0) {
            channel = PV_TO_CHANNEL_MAP[i].channel;
            break;
        }
    }   

    if (channel == NO_CHANNEL){ 
        return -1; 
    }

    // Find the index of the channel in the list
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (channel_configs[i].channel == channel) {
            return i;  
        }
    }

    return -1;
}


/**
 * A function to update the log PV with the latest error message. Causes the 
 * waveform PV pLog to process. 
 * 
 * @param pv_name The name of the PV processing when error occured. 
 *           error_message Message to go with error. 
 *           status_code The status code from Picoscope API. 
 */
void log_message(struct PS6000AModule* mp, char pv_name[], char error_message[], uint32_t status_code){
    
    int16_t max_log_message = mp->pLog->nelm; 
    char log_message[max_log_message]; 
    
    int size = sprintf(log_message, "%s - %s Status code: 0x%08X", pv_name, error_message, status_code);

    if (size >= 0) {
        epicsMutexLock(mp->pLog->mlok);
        memcpy(mp->pLog->bptr, log_message, strlen(log_message)+1);
        mp->pLog->nord = strlen(log_message)+1;
        dbProcess((struct dbCommon *) mp->pLog);
        epicsMutexUnlock(mp->pLog->mlok);
    }
}


#include <mbbiRecord.h>
#include <mbboRecord.h>

/**
 * Function to update the enum string and values of mbbo and mbbi records. 
 * 
 * @param pmbbo A pointer to a PV of type mbboRecord. 
 *        pmbbi A pointer to a PV of type mbbiRecord. 
 *        options Contains new string and values for each enum option. 
 */
void update_enum_options(struct mbboRecord* pmbbo, struct mbbiRecord* pmbbi, MultiBitBinaryEnums options){ 
    
    void update_field(char* pmbbo_str, char* pmbbi_str, int pmbbo_val, int pmbbi_val, const char* new_str, int new_val) {
        if (new_str){
            memcpy(pmbbo_str, new_str, strlen(new_str)+1);
            memcpy(pmbbi_str, new_str, strlen(new_str)+1);
            pmbbo_val = new_val; 
            pmbbi_val = new_val; 
        }
    }

    update_field(pmbbo->zrst, pmbbi->zrst, pmbbo->zrvl, pmbbi->zrvl, options.zrst, options.zrvl); 
    update_field(pmbbo->onst, pmbbi->onst, pmbbo->onvl, pmbbi->onvl, options.onst, options.onvl); 
    update_field(pmbbo->twst, pmbbi->twst, pmbbo->twvl, pmbbi->twvl, options.twst, options.twvl); 
    update_field(pmbbo->thst, pmbbi->thst, pmbbo->thvl, pmbbi->thvl, options.thst, options.thvl); 
    update_field(pmbbo->frst, pmbbi->frst, pmbbo->frvl, pmbbi->frvl, options.frst, options.frvl); 
    update_field(pmbbo->fvst, pmbbi->fvst, pmbbo->fvvl, pmbbi->fvvl, options.fvst, options.fvvl); 
    update_field(pmbbo->sxst, pmbbi->sxst, pmbbo->sxvl, pmbbi->sxvl, options.sxst, options.sxvl); 
    update_field(pmbbo->svst, pmbbi->svst, pmbbo->svvl, pmbbi->svvl, options.svst, options.svvl); 
    update_field(pmbbo->eist, pmbbi->eist, pmbbo->eivl, pmbbi->eivl, options.eist, options.eivl); 
    update_field(pmbbo->nist, pmbbi->nist, pmbbo->nivl, pmbbi->nivl, options.nist, options.nivl); 
    update_field(pmbbo->test, pmbbi->test, pmbbo->tevl, pmbbi->tevl, options.test, options.tevl); 
    update_field(pmbbo->elst, pmbbi->elst, pmbbo->elvl, pmbbi->elvl, options.elst, options.elvl); 
    update_field(pmbbo->tvst, pmbbi->tvst, pmbbo->tvvl, pmbbi->tvvl, options.tvst, options.tvvl); 
    update_field(pmbbo->ttst, pmbbi->ttst, pmbbo->ttvl, pmbbi->ttvl, options.ttst, options.ttvl); 
    update_field(pmbbo->ftst, pmbbi->ftst, pmbbo->ftvl, pmbbi->ftvl, options.ftst, options.ftvl); 
    update_field(pmbbo->ffst, pmbbi->ffst, pmbbo->ffvl, pmbbi->ffvl, options.ffst, options.ffvl); 

}