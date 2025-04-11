#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsExport.h>
#include <errlog.h>
#include <sys/time.h>

#include <aiRecord.h>
#include <aoRecord.h>

#include "devPicoscopeCommon.h"

#define MAX_SAMPLE_SIZE 1000000

enum ioType
    {
    UNKNOWN_IOTYPE, // default case, must be 0 
    SET_NUM_SAMPLES,
    GET_NUM_SAMPLES,
    SET_TRIGGER_PULSE_WIDTH,
    GET_TRIGGER_PULSE_WIDTH,
    SET_DOWN_SAMPLE_RATIO,
    GET_DOWN_SAMPLE_RATIO,
    SET_TRIGGER_POSITION_RATIO,
    GET_TRIGGER_POSITION_RATIO,
    SET_ANALOG_OFFSET,
    GET_ANALOG_OFFSET,
    GET_ACQUISITION_STATUS,
    SET_TRIGGER_UPPER,
    GET_TRIGGER_UPPER,
    SET_TRIGGER_UPPER_HYSTERESIS,
    GET_TRIGGER_UPPER_HYSTERESIS,
    SET_TRIGGER_LOWER,
    GET_TRIGGER_LOWER,
    SET_TRIGGER_LOWER_HYSTERESIS,
    GET_TRIGGER_LOWER_HYSTERESIS,
    GET_SAMPLE_INTERVAL,
    SET_NUM_DIVISIONS,
    GET_NUM_DIVISIONS, 
    GET_SAMPLE_RATE, 
    GET_TIMEBASE,
    SET_AUTO_TRIGGER_US, 
    GET_AUTO_TRIGGER_US,
    GET_TRIGGER_FREQUENCY
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
        {"set_num_samples", isOutput, SET_NUM_SAMPLES, ""},
        {"get_num_samples", isInput, GET_NUM_SAMPLES, ""},
        {"set_trigger_pulse_width", isOutput, SET_TRIGGER_PULSE_WIDTH, ""},
        {"get_trigger_pulse_width", isInput, GET_TRIGGER_PULSE_WIDTH, ""},
        {"get_timebase", isInput, GET_TIMEBASE, ""},
        {"set_down_sampling_ratio", isOutput, SET_DOWN_SAMPLE_RATIO, ""},
        {"get_down_sampling_ratio", isInput, GET_DOWN_SAMPLE_RATIO, ""},
        {"set_trigger_position_ratio", isOutput, SET_TRIGGER_POSITION_RATIO, "" },
        {"get_trigger_position_ratio", isInput, GET_TRIGGER_POSITION_RATIO, "" },
        {"set_analog_offset", isOutput, SET_ANALOG_OFFSET, ""},
        {"get_analog_offset", isInput, GET_ANALOG_OFFSET, ""},
        {"get_acquisition_status", isInput, GET_ACQUISITION_STATUS, "" },
        {"set_trigger_upper", isOutput, SET_TRIGGER_UPPER, ""},
        {"get_trigger_upper", isInput, GET_TRIGGER_UPPER, ""},
        {"set_trigger_upper_hysteresis", isOutput, SET_TRIGGER_UPPER_HYSTERESIS, ""},
        {"get_trigger_upper_hysteresis", isInput, GET_TRIGGER_UPPER_HYSTERESIS, ""},
        {"set_trigger_lower", isOutput, SET_TRIGGER_LOWER, ""},
        {"get_trigger_lower", isInput, GET_TRIGGER_LOWER, ""},
        {"set_trigger_lower_hysteresis", isOutput, SET_TRIGGER_LOWER_HYSTERESIS, ""},
        {"get_trigger_lower_hysteresis", isInput, GET_TRIGGER_LOWER_HYSTERESIS, ""},
        {"get_sample_interval", isInput, GET_SAMPLE_INTERVAL, "" },
        {"get_sample_rate", isInput, GET_SAMPLE_RATE, ""},
        {"set_num_divisions", isOutput, SET_NUM_DIVISIONS, ""},
        {"get_num_divisions", isInput, GET_NUM_DIVISIONS, ""},
        {"set_auto_trigger_us", isOutput, SET_AUTO_TRIGGER_US, ""},
        {"get_auto_trigger_us", isInput, GET_AUTO_TRIGGER_US, ""},
        {"get_trigger_frequency", isInput, GET_TRIGGER_FREQUENCY, ""}
    };

#define AIO_TYPE_SIZE    (sizeof (AioType) / sizeof (struct aioType))

struct PicoscopeAioData
    {
        char serial_num[10]; 
        enum ioType ioType;
        char *cmdPrefix;
        char paramLabel[32];
        int paramValid;
        struct PS6000AModule* mp;
    };

static enum ioType findAioType(enum ioFlag ioFlag, char *param, char **cmdString)
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
 * AI Record
 ****************************************************************************************/

typedef long (*DEVSUPFUN_AI)(struct aiRecord *);

static long init_record_ai (struct aiRecord *pai);
static long read_ai (struct aiRecord *pai);

struct
{
    long         number;
    DEVSUPFUN_AI report;
    DEVSUPFUN_AI init;
    DEVSUPFUN_AI init_record;
    DEVSUPFUN_AI get_ioint_info;
    DEVSUPFUN_AI read;
} devPicoscopeAi =
    {
        6,
        NULL,
        NULL,
        init_record_ai,
        NULL,
        read_ai,
    };

epicsExportAddress(dset, devPicoscopeAi);


static long init_record_ai (struct aiRecord *pai)
{    
    struct instio  *pinst;
    struct PicoscopeAioData *vdp;

    if (pai->inp.type != INST_IO)
    {
        errlogPrintf("%s: INP field type should be INST_IO\n", pai->name);
        return(S_db_badField);
    }
    pai->dpvt = calloc(sizeof(struct PicoscopeAioData), 1);
    if (pai->dpvt == (void *)0){
        errlogPrintf("%s: Failed to allocated memory\n", pai->name);
       return -1;
    }

    pinst = &(pai->inp.value.instio);
    vdp = (struct PicoscopeAioData *)pai->dpvt;

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0){
        printf("Error when getting function name: %s\n",vdp->paramLabel);
        return -1;
    }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

    vdp->ioType = findAioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));
    if (vdp->ioType == UNKNOWN_IOTYPE){
        errlogPrintf("%s: Invalid type: \"@%s\"\n", pai->name, vdp->paramLabel);
        return(S_db_badField);
    }

    switch(vdp->ioType)
    {
        case GET_TRIGGER_UPPER:
            vdp->mp->pTriggerThresholdFbk[0] = pai;
            break;

        case GET_TRIGGER_UPPER_HYSTERESIS: 
            vdp->mp->pTriggerThresholdFbk[1] = pai; 
            break; 

        case GET_TRIGGER_LOWER:
            vdp->mp->pTriggerThresholdFbk[2] = pai;
            break;

        case GET_TRIGGER_LOWER_HYSTERESIS: 
            vdp->mp->pTriggerThresholdFbk[3] = pai; 
            break;

        case GET_TRIGGER_FREQUENCY: 
            vdp->mp->pTriggerFrequency = pai; 
            break;

        default:
            return 2;
    } 
    
    return 0;
}


static long read_ai (struct aiRecord *pai){
        
    char* record_name; 
    int channel_index; 

    struct PicoscopeAioData *vdp = (struct PicoscopeAioData *)pai->dpvt;

    switch (vdp->ioType)
    {
        case GET_ANALOG_OFFSET: 
            record_name = pai->name; 
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs); 

            pai->val = vdp->mp->channel_configs[channel_index].analog_offset; 
            break; 

        case GET_NUM_SAMPLES: 
            pai->val = vdp->mp->sample_config.num_samples;
            break; 

        case GET_TRIGGER_PULSE_WIDTH: 
            pai->val = vdp->mp->trigger_config.AUXTriggerSignalPulseWidth;
            break;

        case GET_DOWN_SAMPLE_RATIO: 
            pai->val = vdp->mp->sample_config.down_sample_ratio; 
            break; 

        case GET_TRIGGER_POSITION_RATIO: 
            pai->val = vdp->mp->sample_config.trigger_position_ratio;
            break; 

        case GET_NUM_DIVISIONS: 
            pai->val = vdp->mp->sample_config.timebase_configs.num_divisions;
            break; 
        
        case GET_SAMPLE_RATE: 
            pai->val = vdp->mp->sample_config.timebase_configs.sample_rate; 
            break;
            
        case GET_TIMEBASE: 
            pai->val = vdp->mp->sample_config.timebase_configs.timebase; 
            break; 
        
        case GET_SAMPLE_INTERVAL: 
            pai->val = vdp->mp->sample_config.timebase_configs.sample_interval_secs; 
            break; 
        
        case GET_ACQUISITION_STATUS:
            pai->val = (float)vdp->mp->dataAcquisitionFlag;
            break;

        case GET_TRIGGER_UPPER:
            pai->val = vdp->mp->trigger_config.thresholdUpperVolts;
            break;

        case GET_TRIGGER_UPPER_HYSTERESIS: 
            pai->val = vdp->mp->trigger_config.thresholdUpperHysteresis;
            break;

        case GET_TRIGGER_LOWER:
            pai->val = vdp->mp->trigger_config.thresholdLowerVolts;
            break;
        
        case GET_TRIGGER_LOWER_HYSTERESIS: 
            pai->val = vdp->mp->trigger_config.thresholdLowerHysteresis; 
            break;

        case GET_AUTO_TRIGGER_US: 
            pai->val = vdp->mp->trigger_config.autoTriggerMicroSeconds; 
            break; 

        case GET_TRIGGER_FREQUENCY: 
            pai->val = vdp->mp->trigger_timing_info.trigger_freq_secs;  
            break;

        default:
            return 2;

    }
    return 2;
}    

/****************************************************************************************
 * AO Record
 ****************************************************************************************/
typedef long (*DEVSUPFUN_AO)(struct aoRecord *);

static long init_record_ao(struct aoRecord *pao);
static long write_ao (struct aoRecord *pao);

struct
{
    long         number;
    DEVSUPFUN_AO report;
    DEVSUPFUN_AO init;
    DEVSUPFUN_AO init_record;
    DEVSUPFUN_AO get_ioint_info;
    DEVSUPFUN_AO write_lout;
} devPicoscopeAo =
    {
        6,
        NULL,
        NULL,
        init_record_ao,
        NULL,
        write_ao, 
    };

epicsExportAddress(dset, devPicoscopeAo);

static long init_record_ao (struct aoRecord *pao)
{    
    struct instio  *pinst;
    struct PicoscopeAioData *vdp;

    if (pao->out.type != INST_IO)
    {
        errlogPrintf("%s: INP field type should be INST_IO\n", pao->name);
        return(S_db_badField);
    }
    pao->dpvt = calloc(sizeof(struct PicoscopeAioData), 1);
    if (pao->dpvt == (void *)0)
    {
        errlogPrintf("%s: Failed to allocated memory\n", pao->name);
        return -1;
    }
  
    pinst = &(pao->out.value.instio);
    vdp = (struct PicoscopeAioData *)pao->dpvt;

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

    vdp->ioType = findAioType(isOutput, vdp->paramLabel, &(vdp->cmdPrefix));

    if (vdp->ioType == UNKNOWN_IOTYPE)
    {
        errlogPrintf("%s: Invalid type: \"%s\"\n", pao->name, vdp->paramLabel);
        return(S_db_badField);
    }
    pao->udf = FALSE;
    vdp->mp->channel_configs[0].channel = CHANNEL_A;
    vdp->mp->channel_configs[1].channel = CHANNEL_B;
    vdp->mp->channel_configs[2].channel = CHANNEL_C;
    vdp->mp->channel_configs[3].channel = CHANNEL_D;

    switch (vdp->ioType)    
    {    
        case SET_NUM_SAMPLES: 
            vdp->mp->sample_config.num_samples = (int)pao->val;
            vdp->mp->sample_config.unadjust_num_samples = (int) pao->val;
            break; 

        case SET_TRIGGER_PULSE_WIDTH: 
            vdp->mp->trigger_config.AUXTriggerSignalPulseWidth = (double)pao->val;
            break; 

        case SET_ANALOG_OFFSET: 
            char* record_name = pao->name;
            int channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     
            
            vdp->mp->pAnalogOffestRecords[channel_index] = pao; 
            break;

        case SET_DOWN_SAMPLE_RATIO: 
            vdp->mp->sample_config.down_sample_ratio = (int)pao->val; 
            break; 
        
        case SET_TRIGGER_POSITION_RATIO:
            vdp->mp->sample_config.trigger_position_ratio = (int)pao->val;
            break;

        case SET_NUM_DIVISIONS: 
            vdp->mp->sample_config.timebase_configs.num_divisions = (int) pao->val; 

            // Initialize timebase feedback information to 0. 
            vdp->mp->sample_config.timebase_configs.timebase = 0; 
            vdp->mp->sample_config.timebase_configs.sample_interval_secs = 0; 
            vdp->mp->sample_config.timebase_configs.sample_rate = 0; 
            break;

        case SET_TRIGGER_UPPER:
            vdp->mp->pTriggerThreshold[0] = pao; 
            vdp->mp->trigger_config.thresholdUpper = (int16_t) pao->val;
            break;
        
        case SET_TRIGGER_UPPER_HYSTERESIS: 
            vdp->mp->trigger_config.thresholdUpperHysteresis = (uint16_t) pao->val; 
            break; 

        case SET_TRIGGER_LOWER:
            vdp->mp->pTriggerThreshold[1] = pao; 
            vdp->mp->trigger_config.thresholdLower = (int16_t) pao->val;
            break;
        
        case SET_TRIGGER_LOWER_HYSTERESIS: 
            vdp->mp->trigger_config.thresholdLowerHysteresis = (uint16_t) pao->val;
            break;

        case SET_AUTO_TRIGGER_US: 
            vdp->mp->trigger_config.autoTriggerMicroSeconds = (uint32_t) pao->val; 
            break; 

        default:
            return 0;
    }

    return 2;
}


static long write_ao (struct aoRecord *pao)
{    
    uint32_t timebase = 0; 
    double sample_interval, sample_rate = 0; 
    int16_t channel_status = 0;
    int returnState = 0;
    
    char* record_name; 
    int channel_index; 
    uint32_t result;

    struct PicoscopeAioData *vdp;
    vdp = (struct PicoscopeAioData *)pao->dpvt;
    uint64_t previous_num_samples;
    switch (vdp->ioType)
    {
        case SET_NUM_DIVISIONS: 
            int16_t previous_num_divisions = vdp->mp->sample_config.timebase_configs.num_divisions; 
            vdp->mp->sample_config.timebase_configs.num_divisions = (int) pao->val; 

            result = get_valid_timebase_configs(
                vdp->mp,
                &sample_interval, 
                &timebase, 
                &sample_rate
            );

            if (result != 0) {
                log_message(vdp->mp, pao->name, "Error setting the number of divisions.", result);
                vdp->mp->sample_config.timebase_configs.num_divisions = previous_num_divisions; 
                break; 
            }

            vdp->mp->sample_config.timebase_configs.sample_interval_secs = sample_interval;
            vdp->mp->sample_config.timebase_configs.timebase = timebase;
            vdp->mp->sample_config.timebase_configs.sample_rate = sample_rate;  
            break; 

        case SET_NUM_SAMPLES:

            previous_num_samples = vdp->mp->sample_config.num_samples; 
            vdp->mp->sample_config.num_samples = (int) pao->val; 
            vdp->mp->sample_config.unadjust_num_samples = (int) pao->val; 
            result = get_valid_timebase_configs(
                vdp->mp,
                &sample_interval, 
                &timebase, 
                &sample_rate
            ); 

            if (result != 0) {
                log_message(vdp->mp, pao->name, "Error setting the number of samples.", result);
                vdp->mp->sample_config.num_samples = previous_num_samples;
                vdp->mp->sample_config.unadjust_num_samples = (int) pao->val; 
                break; 
            }

            vdp->mp->sample_config.timebase_configs.sample_interval_secs = sample_interval;
            vdp->mp->sample_config.timebase_configs.timebase = timebase;
            vdp->mp->sample_config.timebase_configs.sample_rate = sample_rate;
            break;  

        case SET_TRIGGER_PULSE_WIDTH:
            previous_num_samples = vdp->mp->sample_config.num_samples; 
            vdp->mp->trigger_config.AUXTriggerSignalPulseWidth = (double)pao->val;
            result = get_valid_timebase_configs(
                vdp->mp,
                &sample_interval, 
                &timebase, 
                &sample_rate
            );
            if (result != 0) {
                log_message(vdp->mp, pao->name, "Error setting the width of trigger signal.", result);
                vdp->mp->sample_config.num_samples = previous_num_samples;
                vdp->mp->sample_config.unadjust_num_samples = (int) pao->val; 
                break; 
            }

            vdp->mp->sample_config.timebase_configs.sample_interval_secs = sample_interval;
            vdp->mp->sample_config.timebase_configs.timebase = timebase;
            vdp->mp->sample_config.timebase_configs.sample_rate = sample_rate;
            break;

        case SET_DOWN_SAMPLE_RATIO: 
            vdp->mp->sample_config.down_sample_ratio = (int)pao->val; 
            break; 
        
        case SET_TRIGGER_POSITION_RATIO:
            vdp->mp->sample_config.trigger_position_ratio = (float)pao->val;
            break;  
        
        case SET_ANALOG_OFFSET: 
            record_name = pao->name;
            channel_index = find_channel_index_from_record(record_name, vdp->mp->channel_configs);     
        
            int16_t previous_analog_offset = vdp->mp->channel_configs[channel_index].coupling;

            double max_analog_offset = 0; 
            double min_analog_offset = 0; 
            result = get_analog_offset_limits(
                vdp->mp->channel_configs[channel_index], 
                vdp->mp->handle,
                &max_analog_offset, 
                &min_analog_offset
            );
            if (result != 0) {
                log_message(vdp->mp, pao->name, "Error getting analog offset limits.", result);
            }

            pao->drvh = max_analog_offset; 
            pao->drvl = min_analog_offset;

            if (pao->val > max_analog_offset) {
                vdp->mp->channel_configs[channel_index].analog_offset = max_analog_offset;
            }
            else if (pao->val < min_analog_offset) { 
                vdp->mp->channel_configs[channel_index].analog_offset = min_analog_offset;
            }
            else {
                vdp->mp->channel_configs[channel_index].analog_offset = pao->val; 
            }  

            channel_status = get_channel_status(vdp->mp->channel_configs[channel_index].channel, vdp->mp->channel_status); 
            if (channel_status == 1) {
                result = set_channel_on(
                    vdp->mp->channel_configs[channel_index], 
                    vdp->mp->handle, 
                    &vdp->mp->channel_status
                );               
                // If channel is not succesfully set on, return to previous value 
                if (result != 0) {
                    log_message(vdp->mp, pao->name, "Error setting analog offset.", result);
                    vdp->mp->channel_configs[channel_index].analog_offset = previous_analog_offset;
                }
            }
            break;
        
        case SET_TRIGGER_UPPER:
            if (vdp->mp->trigger_config.channel == TRIGGER_AUX || vdp->mp->trigger_config.triggerType == NO_TRIGGER) {
                vdp->mp->trigger_config.thresholdUpper = 0;
                vdp->mp->trigger_config.thresholdUpperVolts = 0; 
                pao->val = 0; 
                break;
            }

            int16_t upper_scaled; 
            result = calculate_scaled_value(
                pao->val, 
                vdp->mp->channel_configs[vdp->mp->trigger_config.channel].range, 
                &upper_scaled, 
                vdp->mp->handle
            );
            if (result != 0) { 
                log_message(vdp->mp, pao->name, "Threshold requested is outside of the trigger channel range.", result); 
                vdp->mp->trigger_config.thresholdUpper = 0; 
                vdp->mp->trigger_config.thresholdUpperVolts = 0; 
                pao->val = 0; 
            } else {
                vdp->mp->trigger_config.thresholdUpperVolts = pao->val;
                vdp->mp->trigger_config.thresholdUpper = upper_scaled;
            }
            break;

        case SET_TRIGGER_UPPER_HYSTERESIS: 
            vdp->mp->trigger_config.thresholdUpperHysteresis = (uint16_t) pao->val; 
            if (vdp->mp->trigger_config.triggerType == NO_TRIGGER){
                vdp->mp->trigger_config.thresholdUpperHysteresis = 0; 
            } 
            break;

        case SET_TRIGGER_LOWER:
            if (vdp->mp->trigger_config.channel == TRIGGER_AUX || vdp->mp->trigger_config.triggerType == NO_TRIGGER) {
                vdp->mp->trigger_config.thresholdLower = 0; 
                vdp->mp->trigger_config.thresholdLowerVolts = 0; 
                pao->val = 0; 
                break;
            }

            int16_t lower_scaled; 
            result = calculate_scaled_value(
                pao->val, 
                vdp->mp->channel_configs[vdp->mp->trigger_config.channel].range, 
                &lower_scaled, 
                vdp->mp->handle
            );
            if (result != 0){ 
                log_message(vdp->mp, pao->name, "Threshold requested is outside of the trigger channel range.", result); 
                vdp->mp->trigger_config.thresholdLower = 0; 
                vdp->mp->trigger_config.thresholdLowerVolts = 0; 
                pao->val = 0; 
            } else {
                vdp->mp->trigger_config.thresholdLowerVolts = pao->val;            
                vdp->mp->trigger_config.thresholdLower = lower_scaled;
            }
            break;

        case SET_TRIGGER_LOWER_HYSTERESIS: 
            vdp->mp->trigger_config.thresholdLowerHysteresis = (uint16_t) pao->val; 
            if (vdp->mp->trigger_config.triggerType == NO_TRIGGER){
                vdp->mp->trigger_config.thresholdLowerHysteresis = 0; 
            } 
            break;

        case SET_AUTO_TRIGGER_US: 
            vdp->mp->trigger_config.autoTriggerMicroSeconds = (uint32_t) pao->val; 
            if (vdp->mp->trigger_config.triggerType == NO_TRIGGER){ 
                vdp->mp->trigger_config.autoTriggerMicroSeconds = 0; 
            } 
            break; 

        default:
                returnState = -1;
    }

    if (returnState < 0)
    {
        if (recGblSetSevr(pao, READ_ALARM, INVALID_ALARM)  &&  errVerbose
            &&  (pao->stat != READ_ALARM  ||  pao->sevr != INVALID_ALARM))
            {
                errlogPrintf("%s: Read Error\n", pao->name);
            }
        return 2;
    }else{
        re_acquire_waveform(vdp->mp);
    }
    return 0;
}