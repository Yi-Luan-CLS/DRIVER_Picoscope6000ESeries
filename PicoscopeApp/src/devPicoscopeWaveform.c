#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <cantProceed.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <recSup.h>
#include <recGbl.h>
#include <devSup.h>
#include <link.h>
#include <epicsTypes.h>
#include <alarm.h>
#include <menuConvert.h>
#include <epicsExport.h>
#include <errlog.h>
#include <waveformRecord.h>
#include "devPicoscopeCommon.h"

enum ioType
    {
    UNKNOWN_IOTYPE, // default case, must be 0 
    START_RETRIEVE_WAVEFORM,
    STOP_RETRIEVE_WAVEFORM,
    UPDATE_WAVEFORM,
    GET_LOG, 
    };
enum ioFlag
    {
    isOutput = 0,
    isInput = 1
    };
static struct aioType
    {
        char *label;
        enum ioFlag flag;
        enum ioType ioType;
        char *cmdp;
    } AioType[] =
    {
        {"start_retrieve_waveform", isInput, START_RETRIEVE_WAVEFORM, "" },
        {"stop_retrieve_waveform", isInput, STOP_RETRIEVE_WAVEFORM, "" },
        {"get_log", isInput, GET_LOG, ""}, 
        {"update_waveform", isInput, UPDATE_WAVEFORM, "" },
    };

#define AIO_TYPE_SIZE    (sizeof (AioType) / sizeof (struct aioType))
struct PicoscopeData
    {
        char serial_num[10]; 
        enum ioType ioType;
        char *cmdPrefix;
        char paramLabel[32];
        int paramValid;
        struct PS6000AModule* mp;
    };
static enum ioType findAioType(enum ioFlag ioFlag, char *param, char **cmdString);
static enum ioType
findAioType(enum ioFlag ioFlag, char *param, char **cmdString)
{
    unsigned int i;

    for (i = 0; i < AIO_TYPE_SIZE; i ++)
        if (strcmp(param, AioType[i].label) == 0  &&  AioType[i].flag == ioFlag)
            {
            *cmdString = AioType[i].cmdp;
            return AioType[i].ioType;
            }

    return UNKNOWN_IOTYPE;
}

/****************************************************************************************
  * Waveform - read a data array of values
 ****************************************************************************************/

#include <waveformRecord.h>

typedef long (*DEVSUPFUN_WAVEFORM)(struct waveformRecord *);


static long init_record_waveform(struct waveformRecord *);
static long read_waveform(struct waveformRecord *);

struct{
    long         number;
    DEVSUPFUN_WAVEFORM report;
    DEVSUPFUN_WAVEFORM init;
    DEVSUPFUN_WAVEFORM init_record;
    DEVSUPFUN_WAVEFORM get_ioint_info;
    // DEVSUPFUN_WAVEFORM write_lout;
    DEVSUPFUN_WAVEFORM read;
    
} devPicoscopeWaveform = {
    6,
    NULL,
    NULL,
    init_record_waveform,
    NULL,
    read_waveform,
};

epicsExportAddress(dset, devPicoscopeWaveform);

static long init_record_waveform(struct waveformRecord * pwaveform)
{
    struct instio  *pinst;
    struct PicoscopeData *vdp;

    if (pwaveform->inp.type != INST_IO)
    {
        errlogPrintf("%s: INP field type should be INST_IO\n", pwaveform->name);
        return(S_db_badField);
    }
    pwaveform->dpvt = calloc(sizeof(struct PicoscopeData), 1);
    if (pwaveform->dpvt == (void *)0)
    {
        errlogPrintf("%s: Failed to allocated memory\n", pwaveform->name);
        return -1;
    }

    pinst = &(pwaveform->inp.value.instio);
    vdp = (struct PicoscopeData *)pwaveform->dpvt;

   
    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

    vdp->ioType = findAioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));
    if (vdp->ioType == UNKNOWN_IOTYPE)
    {
        errlogPrintf("%s: Invalid type: \"@%s\"\n", pwaveform->name, vdp->paramLabel);
        return(S_db_badField);
    }

    pwaveform->udf = FALSE;

    switch (vdp->ioType)
    {    

        case START_RETRIEVE_WAVEFORM:
            vdp->mp->pWaveformStartPtr = pwaveform;
            break;
      
        case GET_LOG: 
            // Save log PV to process when errors occur
            vdp->mp->pLog = pwaveform; 
            break; 

        case UPDATE_WAVEFORM:
            int channel_index = find_channel_index_from_record(pwaveform->name, vdp->mp->channel_configs); 
            vdp->mp->pRecordUpdateWaveform[channel_index] = pwaveform;
            vdp->mp->waveform[channel_index] = calloc(pwaveform->nelm, sizeof(int16_t));
            if (!vdp->mp->waveform[channel_index]) {
                errlogPrintf("%s: Waveform memory allocation failed\n", pwaveform->name);
                return -1;
            }
            break;

        case STOP_RETRIEVE_WAVEFORM:
            vdp->mp->pWaveformStopPtr = pwaveform;
            break;

        default:
            return 0;
    }

    return 0;
}

void
captureThreadFunc(void *arg) {
    PS6000AModule* mp = (struct PS6000AModule *)arg;
    epicsMutexLock(mp->epics_acquisition_thread_mutex);
    epicsThreadId id = epicsThreadGetIdSelf();
    printf("Start ID is %ld\n", id->tid);
    // Setup Picoscope
    uint32_t status = setup_picoscope(mp);
    if (status != 0) {
        log_message(mp, "", "Error configuring picoscope for data capture.", status);
        printf("captureThreadFunc Cleanup ID is %ld\n", id->tid);
        epicsMutexLock(mp->epics_acquisition_flag_mutex);
        mp->dataAcquisitionFlag = 0;
        epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
        epicsMutexUnlock(mp->epics_acquisition_thread_mutex);
        return;
    }

    while (mp->dataAcquisitionFlag == 1) {
        double time_indisposed_ms = 0;

        mp->sample_collected = mp->sample_config.num_samples;
        status = run_block_capture(mp, &time_indisposed_ms);
        if (status != 0) {
            log_message(mp, "", "Error capturing data block.", status);
            break;
        }

        // Process the UPDATE_WAVEFORM subroutine to update waveform
        for (size_t i = 0; i < CHANNEL_NUM; i++) {
            if (get_channel_status(mp->channel_configs[i].channel, mp->channel_status) && mp->pRecordUpdateWaveform[i]) {
                dbProcess((struct dbCommon *)mp->pRecordUpdateWaveform[i]);

            }
        }
    }

    stop_capturing(mp->handle);
    printf("Cleanup ID is %ld\n", id->tid);
    epicsMutexLock(mp->epics_acquisition_flag_mutex);
    mp->dataAcquisitionFlag = 0;
    epicsMutexUnlock(mp->epics_acquisition_flag_mutex);
    epicsMutexUnlock(mp->epics_acquisition_thread_mutex);
}

static long
read_waveform(struct waveformRecord *pwaveform) {
    struct PicoscopeData *vdp = (struct PicoscopeData *)pwaveform->dpvt;

    switch (vdp->ioType) {
        case START_RETRIEVE_WAVEFORM:
            printf("Start Retrieving\n");
            epicsMutexLock(vdp->mp->epics_acquisition_flag_mutex);
            if (vdp->mp->dataAcquisitionFlag) {
                fprintf(stderr, "Capture thread already running\n");
                epicsMutexUnlock(vdp->mp->epics_acquisition_flag_mutex);
                return -1;
            }
            vdp->mp->dataAcquisitionFlag = 1;
            epicsMutexUnlock(vdp->mp->epics_acquisition_flag_mutex);
	        // vdp->mp->triggerReadyEvent = epicsEventCreate(0);

            // Create capture thread
            epicsThreadId capture_thread = epicsThreadCreate("captureThread", epicsThreadPriorityMedium,
                                                             0, (EPICSTHREADFUNC)captureThreadFunc, vdp->mp);
            if (!capture_thread) {
                errlogPrintf("%s: Failed to create capture thread\n", pwaveform->name);
                epicsMutexLock(vdp->mp->epics_acquisition_flag_mutex);
                vdp->mp->dataAcquisitionFlag = 0;
                epicsMutexUnlock(vdp->mp->epics_acquisition_flag_mutex);
                return -1;
            }
            break;

        case UPDATE_WAVEFORM:
            int channel_index = find_channel_index_from_record(pwaveform->name, vdp->mp->channel_configs); 
            epicsMutexLock(vdp->mp->epics_acquisition_flag_mutex);
            if (vdp->mp->dataAcquisitionFlag == 1) {
                memcpy(pwaveform->bptr, vdp->mp->waveform[channel_index], vdp->mp->sample_collected * sizeof(int16_t));
                pwaveform->nord = vdp->mp->sample_collected;
            }
            epicsMutexUnlock(vdp->mp->epics_acquisition_flag_mutex);
            break;

        case STOP_RETRIEVE_WAVEFORM:
            epicsMutexLock(vdp->mp->epics_acquisition_flag_mutex);
            vdp->mp->dataAcquisitionFlag = 0;
            epicsMutexUnlock(vdp->mp->epics_acquisition_flag_mutex);
    		epicsEventSignal(vdp->mp->triggerReadyEvent);
            break;

        default:
            if (recGblSetSevr(pwaveform, READ_ALARM, INVALID_ALARM) && errVerbose
                && (pwaveform->stat != READ_ALARM || pwaveform->sevr != INVALID_ALARM)) {
                errlogPrintf("%s: Read Error\n", pwaveform->name);
            }
            return -1;
    }

    return 0;
}
