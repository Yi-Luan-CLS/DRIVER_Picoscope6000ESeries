/*
 * ---------------------------------------------------------------------
 * File:
 *     devPicoscopeBio.c
 * Description:
 *     Device support for EPICS bi and bo records for Picoscope PS6000A 
 *     module. Supports status monitoring and control operations such as 
 *     device opening, channel toggling, and connectivity checks.
 * ---------------------------------------------------------------------
 * Copyright (c) 2025 Canadian Light Source Inc.
 *
 * This file is part of DRIVER_Picoscope6000ESeries.
 *
 * It is licensed under the GNU General Public License v3.0.
 * See the LICENSE.md file in the project root, or visit:
 * https://www.gnu.org/licenses/gpl-3.0.html
 *
 * This software is provided WITHOUT WARRANTY of any kind.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsExport.h>
#include <errlog.h>

#include <biRecord.h>
#include <boRecord.h>

#include "devPicoscopeCommon.h"


enum ioType
{
    UNKNOWN_IOTYPE, // default case, must be 0 
    OPEN_PICOSCOPE,
    GET_DEVICE_STATUS,
    SET_CHANNEL_ON,
    GET_CHANNEL_STATUS,
};

enum ioFlag
{
    isOutput = 0,
    isInput = 1
};

struct bioType 
{
    char *label; 
    enum ioFlag flag; 
    enum ioType ioType; 
    int onCmd; 
    int offCmd;
} BioType [] =
{
    {"open_picoscope",     isOutput,   OPEN_PICOSCOPE,      1,    0 },
    {"get_device_status",  isInput,    GET_DEVICE_STATUS,   1,    0 },
    {"set_channel_on",     isOutput,   SET_CHANNEL_ON,      1,    0 },
    {"get_channel_status", isInput,    GET_CHANNEL_STATUS,  1,    0 },

};

#define BIO_TYPE_SIZE    (sizeof (BioType) / sizeof (struct bioType))

struct PicoscopeBioData
    {   
        char serial_num[10];
        enum ioType ioType;
        char *cmdPrefix;
        char paramLabel[32];
        int paramValid;
        int onCmd;
	    int offCmd;
        struct PS6000AModule* mp;
    };

static enum ioType findBioType(enum ioFlag ioFlag, char *param,  int *onCmd, int *offCmd)
{
	unsigned int i;

	for (i = 0; i < BIO_TYPE_SIZE; i ++){
		if (strncmp(param, BioType[i].label, strlen(BioType[i].label)) == 0  &&
		    BioType[i].flag == ioFlag)
		    	{
			*onCmd  = BioType[i].onCmd;
			*offCmd = BioType[i].offCmd;
			return BioType[i].ioType;
			}
	}
	printf("%s UNKNOWN_IOTYPE\n", param);
	return UNKNOWN_IOTYPE;
}

/****************************************************************************************
 * Binary Output Records - bo
 ****************************************************************************************/

typedef long (*DEVSUPFUN_BO)(struct boRecord *);

static long init_record_bo(struct boRecord *pbo);
static long write_bo(struct boRecord *pbo);

struct
{
	long         number;
	DEVSUPFUN_BO report;
	DEVSUPFUN_BO init;
	DEVSUPFUN_BO init_record;
	DEVSUPFUN_BO get_ioint_info;
	DEVSUPFUN_BO write_bo;
} devPicoscopeBo =
	{
		5,
		NULL,
		NULL,
		init_record_bo,
		NULL,
		write_bo,
	};

epicsExportAddress(dset, devPicoscopeBo);

static long
init_record_bo (struct boRecord *pbo)
{
    char *record_name;
    int channel_index;

    struct instio  *pinst;
	struct PicoscopeBioData *vdp;

    if (pbo->out.type != INST_IO){
            printf("%s: INP field type should be INST_IO\n", pbo->name);
            return(S_db_badField);
    }
    pbo->dpvt = calloc(sizeof(struct PicoscopeBioData), 1);
    if (pbo->dpvt == NULL){
            printf("%s: Failed to allocated memory\n", pbo->name);
            return -1;
    }

    pinst = &(pbo->out.value.instio);
    vdp = (struct PicoscopeBioData *)pbo->dpvt;
   

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

	vdp->ioType = findBioType(isOutput, vdp->paramLabel, &vdp->onCmd, &vdp->offCmd);
    if (vdp->ioType == UNKNOWN_IOTYPE)
    {
        errlogPrintf("%s: Invalid type: \"%s\"\n", pbo->name, vdp->paramLabel);
        return(S_db_badField);
    }

    switch (vdp->ioType) {

        case OPEN_PICOSCOPE: 
            // On initialization open picoscope with default resolution. 
            int16_t handle = 0; 
            int16_t result = open_picoscope(vdp->mp, &handle);

            vdp->mp->handle = handle; 
            break;

        case SET_CHANNEL_ON:    
            record_name = pbo->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 
            
            // On initalization, set all channels off. 
            result = set_channel_off(
                vdp->mp->channel_configs[channel_index].channel, 
                vdp->mp->handle, 
                &vdp->mp->channel_status
            );
            if (result != 0) {
                printf("Error setting channel %s off.\n", record_name);
            }
            break;

        default:
            return -1; 
    }
	
    return 0; 
}

static long
write_bo (struct boRecord *pbo)
{
    uint32_t timebase = 0; 
    double sample_interval, sample_rate = 0;    
	
    struct PicoscopeBioData *vdp = (struct PicoscopeBioData *)pbo->dpvt;
    int returnStatus = -1; 
    int rbv = 1; 
    uint32_t result;

	switch (vdp->ioType){
        case OPEN_PICOSCOPE: 
            int pv_value = (int)pbo->val; 
            char message[100]; 
            
            if (pv_value == 1){
                int16_t handle; 
                result = open_picoscope(vdp->mp, &handle);
                if (result != 0) {
                    sprintf(message, "Error opening picoscope with serial number %s.", vdp->serial_num);
                    log_message(vdp->mp, pbo->name, message, result);
                    rbv = 0; 
                    break;
                }  
                vdp->mp->handle = handle; // Update
            } else {
                if (*vdp->mp->dataAcquisitionFlag == 1) {
                    dbProcess((struct dbCommon *)vdp->mp->pWaveformStopPtr);

                    // this is to make sure the capureting thread is actually stopped
                    epicsMutexLock(vdp->mp->epics_acquisition_thread_mutex);
                    epicsMutexUnlock(vdp->mp->epics_acquisition_thread_mutex);
                    printf("Data acquisition stopped\n");
                }
                result = close_picoscope(vdp->mp); 
                if (result != 0) {
                    sprintf(message, "Error closing picoscope with serial number %s.", vdp->serial_num);
                    log_message(vdp->mp, pbo->name, message, result);
                }   
            }
            break;

        case SET_CHANNEL_ON:    
            char* record_name = pbo->name;
            int channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 
            pv_value = pbo->val;

            // If PV value is 1 (ON) set channel on 
            if (pv_value == 1) { 
                result = set_channel_on(
                    vdp->mp->channel_configs[channel_index], 
                    vdp->mp->handle, 
                    &vdp->mp->channel_status
                );
                if (result != 0) {
                    log_message(vdp->mp, pbo->name, "Error setting channel on.", result);
                    pbo->val = 0; 
                    rbv = 0; 
                }
            } 
            else if (pv_value == 0) {
                result = set_channel_off(
                    vdp->mp->channel_configs[channel_index].channel, 
                    vdp->mp->handle,
                    &vdp->mp->channel_status
                );
                if (result != 0) {
                    log_message(vdp->mp, pbo->name, "Error setting channel off.", result);
                    pbo->val = 0; 
                }
            }    

            // Update timebase configs that are affected by the number of channels on. 
            result = get_valid_timebase_configs(
                vdp->mp,
                &sample_interval, 
                &timebase, 
                &sample_rate
            );                     
            
            if (result != 0){
                log_message(vdp->mp, pbo->name, "Error setting timebase configurations.", result);
            }

            vdp->mp->sample_config.timebase_configs.sample_interval_secs = sample_interval;
            vdp->mp->sample_config.timebase_configs.timebase = timebase;
            vdp->mp->sample_config.timebase_configs.sample_rate = sample_rate;  
            re_acquire_waveform(vdp->mp);
            break;

		
        default:
			returnStatus = -1;
			break;
	}

	if (returnStatus <= 0 ){
		if (recGblSetSevr(pbo, WRITE_ALARM, INVALID_ALARM)  &&  errVerbose
				&& (pbo->stat != WRITE_ALARM  ||  pbo->sevr != INVALID_ALARM))
			printf("%s: Write error (bo)\n\n", pbo->name);
		return 2;
	}
    
    pbo->rbv = rbv;

	return 0;
}


/****************************************************************************************
 * Binary Input Records - bi
 ****************************************************************************************/
typedef long (*DEVSUPFUN_BI)(struct biRecord *);

static long init_bi(int pass);
static long init_record_bi(struct biRecord *pbi);
static long read_bi(struct biRecord *pbi);
    
struct 
{
    long         number;                          
    DEVSUPFUN_BI report;
    long (*init)(int pass);
    DEVSUPFUN_BI init_record;
    DEVSUPFUN_BI get_ioint_info;
    DEVSUPFUN_BI read;
} devPicoscopeBi =
	{
		5,
		NULL,
		init_bi,
		init_record_bi,
		NULL,
		read_bi
	};

epicsExportAddress(dset, devPicoscopeBi);

static long init_bi(int pass)
{
	if (pass >= 1)
		return 0;
	return 0;
}

static long init_record_bi(struct biRecord *pbi)
{   
    struct instio  *pinst;
	struct PicoscopeBioData *vdp;

    if (pbi->inp.type != INST_IO){
            printf("%s: INP field type should be INST_IO\n", pbi->name);
            return(S_db_badField);
    }
    pbi->dpvt = calloc(sizeof(struct PicoscopeBioData), 1);
    if (pbi->dpvt == (void *)0){
            printf("%s: Failed to allocated memory\n", pbi->name);
            return -1;
    }
 
    pinst = &(pbi->inp.value.instio);
    vdp = (struct PicoscopeBioData *)pbi->dpvt;

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

	vdp->ioType = findBioType(isInput, vdp->paramLabel, &vdp->onCmd, &vdp->offCmd);
    if (vdp->ioType == UNKNOWN_IOTYPE)
    {
        errlogPrintf("%s: Invalid type: \"%s\"\n", pbi->name, vdp->paramLabel);
        return(S_db_badField);
    }

	return 0;
}

static long read_bi (struct biRecord *pbi)
{

	struct PicoscopeBioData *vdp;
	vdp = (struct PicoscopeBioData *)pbi->dpvt;

    int returnStatus = -1; 

	switch (vdp->ioType)
		{
        case GET_DEVICE_STATUS:
            uint32_t result = ping_picoscope(vdp->mp); 
            if ( result != 0 ) {
                log_message(vdp->mp, pbi->name, "Cannot ping device.", result);
                pbi->val = 0;
                break;
            }
            pbi->val = 1; 
            pbi->rval = 1; 

            break;

        case GET_CHANNEL_STATUS: 
            char* record_name = pbi->name; 
            int channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 

            int16_t channel_status = get_channel_status(vdp->mp->channel_configs[channel_index].channel, vdp->mp->channel_status); 
            if (channel_status == -1) {
                log_message(vdp->mp, pbi->name, "Cannot get channel status.", channel_status);
            }
            pbi->val = channel_status;
            pbi->rval = channel_status;
            
            break; 

		default:
            returnStatus = -1; 
		}

	if (returnStatus <= 0){
		if (recGblSetSevr(pbi, READ_ALARM, INVALID_ALARM)  &&  errVerbose
		    &&  (pbi->stat != READ_ALARM  ||  pbi->sevr != INVALID_ALARM))
			printf("%s: Read Error\n", pbi->name);
		return 2;
	}

	return 0;
}
