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
    if (mp->dataAcquisitionFlag!=1) {
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
inline int find_channel_index_from_record(const char* record_name, struct ChannelConfigs channel_configs[CHANNEL_NUM]) {
    char channel_str[4];
    sscanf(record_name, "%*[^:]:%4[^:]", channel_str);  // Extract the channel part, e.g., "CHA", "CHB", etc.

    enum Channel channel;
    if (strcmp(channel_str, "CHA") == 0) {
        channel = CHANNEL_A;
    } else if (strcmp(channel_str, "CHB") == 0) {
        channel = CHANNEL_B;
    } else if (strcmp(channel_str, "CHC") == 0) {
        channel = CHANNEL_C;
    } else if (strcmp(channel_str, "CHD") == 0) {
        channel = CHANNEL_D;
    } else {
        printf("Channel not found from record name: %s.\n", record_name); 
        return -1;  // Invalid channel
    }

    // Find the index of the channel in the list
    for (int i = 0; i < 4; i++) {
        if (channel_configs[i].channel == channel) {
            return i;  // Return index if channel matches
        }
    }

    return -1;  // Channel not found
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
    
    int16_t size = snprintf(NULL, 0, "%s - %s Status code: 0x%08X", pv_name, error_message, status_code);

    char log[size+1]; 
    sprintf(log, "%s - %s Status code: 0x%08X", pv_name, error_message, status_code);
    memcpy(mp->pLog->bptr, log, strlen(log)+1);
    mp->pLog->nord = strlen(log)+1;

    dbProcess((struct dbCommon *)mp->pLog);     
    usleep(100); // wait for log PV to process
}