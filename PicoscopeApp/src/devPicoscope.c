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
#include <aiRecord.h>
#include <stringinRecord.h>
#include <menuConvert.h>
#include <epicsExport.h>
#include <errlog.h>
#include <epicsMutex.h>
#include "picoscopeConfig.h"
#include "drvPicoscope.h"

#define MAX_SAMPLE_SIZE 1000000

int16_t result; 
int8_t dataAcquisitionControl = 0;
uint8_t dataAcquisitionFinished;
epicsMutexId epics_acquisition_control_mutex;
epicsMutexId epics_acquisition_thread_mutex;
epicsMutexId epics_acquisition_pv_mutex;

enum ioType
	{
	UNKNOWN_IOTYPE, // default case, must be 0 
	OPEN_PICOSCOPE,
	GET_DEVICE_STATUS,
	SET_RESOLUTION,
	GET_RESOLUTION,
	SET_NUM_SAMPLES,
	GET_NUM_SAMPLES,
	SET_DOWN_SAMPLE_RATIO,
	GET_DOWN_SAMPLE_RATIO,
	SET_DOWN_SAMPLE_RATIO_MODE,
	GET_DOWN_SAMPLE_RATIO_MODE,
	SET_TRIGGER_POSITION_RATIO,
	GET_TRIGGER_POSITION_RATIO,
  	GET_SERIAL_NUM,
  	GET_DEVICE_INFO,
	SET_CHANNEL_ON,
	GET_CHANNEL_STATUS,
	SET_COUPLING,
	GET_COUPLING,
	SET_RANGE, 
	GET_RANGE,
	SET_ANALOG_OFFSET,
	GET_ANALOG_OFFSET,
	GET_MAX_ANALOG_OFFSET,
	GET_MIN_ANALOG_OFFSET,
	SET_BANDWIDTH, 
	GET_BANDWIDTH,
	START_RETRIEVE_WAVEFORM,
	GET_ACQUISITION_STATUS,
	STOP_RETRIEVE_WAVEFORM,
	UPDATE_WAVEFORM,
	DEVICE_TO_OPEN,
	SET_TRIGGER_DIRECTION,
	GET_TRIGGER_DIRECTION,
	SET_TRIGGER_CHANNEL,
	GET_TRIGGER_CHANNEL,
	SET_TRIGGER_MODE,
	GET_TRIGGER_MODE,
	SET_TRIGGER_UPPER,
	GET_TRIGGER_UPPER,
	SET_TRIGGER_LOWER,
	GET_TRIGGER_LOWER,
	GET_SAMPLE_INTERVAL,
	SET_TIME_PER_DIVISION_UNIT, 
	GET_TIME_PER_DIVISION_UNIT, 
	SET_TIME_PER_DIVISION, 
	GET_TIME_PER_DIVISION, 
	SET_NUM_DIVISIONS,
	GET_NUM_DIVISIONS, 
	GET_SAMPLE_RATE, 
	GET_TIMEBASE,
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
		{"open_picoscope", isOutput, OPEN_PICOSCOPE, ""},
		{"get_device_status", isInput, GET_DEVICE_STATUS, ""},
		{"set_resolution", isOutput, SET_RESOLUTION, ""},
		{"get_resolution", isInput, GET_RESOLUTION, ""},
		{"set_num_samples", isOutput, SET_NUM_SAMPLES, ""},
		{"get_num_samples", isInput, GET_NUM_SAMPLES, ""},
		{"get_timebase", isInput, GET_TIMEBASE, ""},
		{"set_down_sampling_ratio", isOutput, SET_DOWN_SAMPLE_RATIO, ""},
		{"get_down_sampling_ratio", isInput, GET_DOWN_SAMPLE_RATIO, ""},
		{"set_down_sampling_ratio_mode", isOutput, SET_DOWN_SAMPLE_RATIO_MODE, ""},
		{"get_down_sampling_ratio_mode", isInput, GET_DOWN_SAMPLE_RATIO_MODE, ""},
		{"set_trigger_position_ratio", isOutput, SET_TRIGGER_POSITION_RATIO, "" },
		{"get_trigger_position_ratio", isInput, GET_TRIGGER_POSITION_RATIO, "" },
	 	{"get_device_info", isInput, GET_DEVICE_INFO, "" },
		{"set_channel_on", isOutput, SET_CHANNEL_ON, ""}, 
		{"get_channel_status", isInput, GET_CHANNEL_STATUS, ""}, 
		{"set_coupling", isOutput, SET_COUPLING, "" },
		{"get_coupling", isInput, GET_COUPLING, ""},
		{"set_range", isOutput, SET_RANGE,   "" }, 
		{"get_range", isInput, GET_RANGE, ""},
		{"set_analog_offset", isOutput, SET_ANALOG_OFFSET, ""},
		{"get_analog_offset", isInput, GET_ANALOG_OFFSET, ""},
		{"get_max_analog_offset", isInput, GET_MAX_ANALOG_OFFSET, ""},
		{"get_min_analog_offset", isInput, GET_MIN_ANALOG_OFFSET, ""},
		{"set_bandwidth", isOutput, SET_BANDWIDTH, "" }, 
		{"get_bandwidth", isInput, GET_BANDWIDTH, "" }, 
		{"start_retrieve_waveform", isInput, START_RETRIEVE_WAVEFORM, "" },
		{"get_acquisition_status", isInput, GET_ACQUISITION_STATUS, "" },
		{"stop_retrieve_waveform", isInput, STOP_RETRIEVE_WAVEFORM, "" },
		{"update_waveform", isInput, UPDATE_WAVEFORM, "" },
		{"device_to_open", isOutput, DEVICE_TO_OPEN, ""},
		{"set_trigger_direction", isOutput, SET_TRIGGER_DIRECTION, ""},
		{"get_trigger_direction", isInput, GET_TRIGGER_DIRECTION, ""},
		{"set_trigger_channel", isOutput, SET_TRIGGER_CHANNEL, ""},
		{"get_trigger_channel", isInput, GET_TRIGGER_CHANNEL, ""},
		{"set_trigger_mode", isOutput, SET_TRIGGER_MODE, ""},
		{"get_trigger_mode", isInput, GET_TRIGGER_MODE, ""},
		{"set_trigger_upper", isOutput, SET_TRIGGER_UPPER, ""},
		{"get_trigger_upper", isInput, GET_TRIGGER_UPPER, ""},
		{"set_trigger_lower", isOutput, SET_TRIGGER_LOWER, ""},
		{"get_trigger_lower", isInput, GET_TRIGGER_LOWER, ""},
		{"set_trigger_direction", isOutput, SET_TRIGGER_DIRECTION, ""}, 
		{"get_sample_interval", isInput, GET_SAMPLE_INTERVAL, "" },
		{"get_sample_rate", isInput, GET_SAMPLE_RATE, ""},
		{"set_time_per_division_unit", isOutput, SET_TIME_PER_DIVISION_UNIT, ""},
		{"get_time_per_division_unit", isInput, GET_TIME_PER_DIVISION_UNIT, ""},
		{"set_time_per_division", isOutput, SET_TIME_PER_DIVISION, ""},
		{"get_time_per_division", isInput, GET_TIME_PER_DIVISION, ""},
		{"set_num_divisions", isOutput, SET_NUM_DIVISIONS, ""},
		{"get_num_divisions", isInput, GET_NUM_DIVISIONS, ""},

    };

#define AIO_TYPE_SIZE    (sizeof (AioType) / sizeof (struct aioType))

struct PicoscopeData
	{
		enum ioType ioType;
		char *cmdPrefix;
		char paramLabel[32];
		int paramValid;
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
struct waveformRecord* pWaveformStart;
struct waveformRecord* pWaveformStop;

int
format_device_support_function(char *string, char *paramName)
{
        if (sscanf(string, "L:%s",paramName) != 1)
                return -1;
        return 0;
}


struct ChannelConfigs* channels[4] = {NULL}; // List of Picoscope channels and their configurations
struct TriggerConfigs* trigger_config = {NULL};
struct SampleConfigs* sample_configurations = NULL; // Configurations for data capture

char* record_name; 
int channel_index; 

/****************************************************************************************
 * AI Record
 ****************************************************************************************/

typedef long (*DEVSUPFUN_AI)(struct aiRecord *);

static long init_record_ai (struct aiRecord *pai);
static long read_ai (struct aiRecord *pai);
// static long special_linconv_ai(struct aiRecord *pai, int after);

struct
	{
	long         number;
	DEVSUPFUN_AI report;
	DEVSUPFUN_AI init;
	DEVSUPFUN_AI init_record;
	DEVSUPFUN_AI get_ioint_info;
	DEVSUPFUN_AI read;
	long (*special_linconv)(struct aiRecord *, int);
	} devPicoscopeAi =
		{
		6,
		NULL,
		NULL,
		init_record_ai,
		NULL,
		read_ai,
        NULL
		};

epicsExportAddress(dset, devPicoscopeAi);

int isInitialised = 0;

int16_t resolution; 

static long
init_record_ai (struct aiRecord *pai)
{	
    struct instio  *pinst;
	struct PicoscopeData *vdp;

    if (pai->inp.type != INST_IO)
    {
        // errlogPrintf("%s: INP field type should be INST_IO\n", pai->name);
        return(S_db_badField);
    }
    pai->dpvt = calloc(sizeof(struct PicoscopeData), 1);
    if (pai->dpvt == (void *)0){
    	// errlogPrintf("%s: Failed to allocated memory\n", pai->name);
	   return -1;
    }

    pinst = &(pai->inp.value.instio);
    vdp = (struct PicoscopeData *)pai->dpvt;

    if (format_device_support_function(pinst->string, vdp->paramLabel) != 0){
		printf("Error when getting function name: %s\n",vdp->paramLabel);
        return -1;
	}

	vdp->ioType = findAioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));

	if (vdp->ioType == UNKNOWN_IOTYPE){
		// errlogPrintf("%s: Invalid type: \"@%s\"\n", pai->name, vdp->paramLabel);
		printf("%s: Invalid type: \"@%s\"\n", pai->name, vdp->paramLabel);
		return(S_db_badField);
	}

	return 0;
}


static long
read_ai (struct aiRecord *pai){

	double max_analog_offset; 
	double min_analog_offset; 

	struct PicoscopeData *vdp = (struct PicoscopeData *)pai->dpvt;

	switch (vdp->ioType)
	{
		// Device configuration fbk
		case GET_DEVICE_STATUS:
			result = ping_picoscope(); 
			pai->val = result;
			break;

		case GET_RESOLUTION: 
			result = get_resolution(&resolution);

			pai->val = resolution;  
			break; 

		// Channel configuration fbk
		case GET_CHANNEL_STATUS: 
			record_name = pai->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			int16_t channel_status = get_channel_status(channels[channel_index]->channel); 
			
			pai->val = channel_status;
			break; 

		case GET_COUPLING: 
			record_name = pai->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			pai->val = channels[channel_index]->coupling;
			break; 

		case GET_RANGE: 
			record_name = pai->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			pai->val = channels[channel_index]->range; 
			break; 

		case GET_BANDWIDTH: 
			record_name = pai->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			pai->val = channels[channel_index]->bandwidth; 
			break; 

		case GET_ANALOG_OFFSET: 
			record_name = pai->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			pai->val = channels[channel_index]->analog_offset; 
			break; 

		case GET_MAX_ANALOG_OFFSET: 
			record_name = pai->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	

			result = get_analog_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analog_offset, &min_analog_offset);

			pai->val = max_analog_offset; 
			break;

		case GET_MIN_ANALOG_OFFSET: 
			record_name = pai->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	

			result = get_analog_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analog_offset, &min_analog_offset);

			pai->val = min_analog_offset; 
			break;

		// Data configuration fbk 
		case GET_NUM_SAMPLES: 
			pai->val = sample_configurations->num_samples; 
			break; 

		case GET_DOWN_SAMPLE_RATIO: 
			pai->val = sample_configurations->down_sample_ratio; 
			break; 

		case GET_DOWN_SAMPLE_RATIO_MODE: 
			pai->val = sample_configurations->down_sample_ratio_mode; 
			
			break;

		case GET_TRIGGER_POSITION_RATIO: 
			pai->val = sample_configurations->trigger_position_ratio;
			break; 

		case GET_NUM_DIVISIONS: 
			pai->val = sample_configurations->timebase_configs.num_divisions;
			break; 
		
		case GET_TIME_PER_DIVISION: 
			pai->val = sample_configurations->timebase_configs.time_per_division; 
			break; 
		
		case GET_TIME_PER_DIVISION_UNIT: 
			pai->val = sample_configurations->timebase_configs.time_per_division_unit; 
			break;
		
		case GET_SAMPLE_RATE: 
			pai->val = sample_configurations->timebase_configs.sample_rate; 
			break;
			
		case GET_TIMEBASE: 
			pai->val = sample_configurations->timebase_configs.timebase; 
			break; 
		
		case GET_SAMPLE_INTERVAL: 
			pai->val = sample_configurations->timebase_configs.sample_interval_secs; 
			break; 
		
		case GET_ACQUISITION_STATUS:
			pai->val = (float)dataAcquisitionControl;
			break;

		case GET_TRIGGER_DIRECTION:
			float direction_value;
			// Handle window mode
			if (trigger_config->thresholdMode == WINDOW) {
			    switch (trigger_config->thresholdDirection) {
			        case INSIDE:
			            direction_value = 11;
			            break;
			        case OUTSIDE:
			            direction_value = 12;
			            break;
			        case ENTER:
			            direction_value = 13;
			            break;
			        case EXIT:
			            direction_value = 14;
			            break;
			        case ENTER_OR_EXIT:
			            direction_value = 15;
			            break;
			        default:
			            direction_value = trigger_config->thresholdDirection;
			            break;
			    }
			} else {
			    direction_value = trigger_config->thresholdDirection;
			}
			
			pai->val = direction_value;
			
			break;
			
		case GET_TRIGGER_CHANNEL: 
			if(trigger_config->channel == TRIGGER_AUX){
				pai->val = 4;
			}else{

				pai->val = trigger_config->channel;
			}
			break;

		case GET_TRIGGER_MODE:
			pai->val = trigger_config->thresholdMode;
			break;

		case GET_TRIGGER_UPPER:
			pai->val = trigger_config->thresholdUpper;
			break;

		case GET_TRIGGER_LOWER:
			pai->val = trigger_config->thresholdLower;
			break;

		default:
			return 2;

	}
	return 2;

}	

/****************************************************************************************
 * AO Record
 ****************************************************************************************/

#include <aoRecord.h>

typedef long (*DEVSUPFUN_AO)(struct aoRecord *);

static long init_record_ao(struct aoRecord *pao);
static long write_ao (struct aoRecord *pao);
void re_acquire_waveform(struct aoRecord *pao){
	if (!dataAcquisitionControl==1) {
		return;
	}
	printf("pao: %s\n", pao->name);
	epicsMutexLock(epics_acquisition_pv_mutex);
	dbProcess((struct dbCommon *)pWaveformStop);
	epicsMutexLock(epics_acquisition_thread_mutex);
	epicsMutexUnlock(epics_acquisition_thread_mutex);
	dbProcess((struct dbCommon *)pWaveformStart);
	epicsMutexUnlock(epics_acquisition_pv_mutex);
}
struct
	{
	long         number;
	DEVSUPFUN_AO report;
	DEVSUPFUN_AO init;
	DEVSUPFUN_AO init_record;
	DEVSUPFUN_AO get_ioint_info;
	DEVSUPFUN_AO write_lout;
	// DEVSUPFUN   special_linconv;
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

int8_t* device_serial_number; 
aoRecord* trigger_pao[3] = {NULL};
struct aoRecord* pAnalogOffestRecords[CHANNEL_NUM];
static long
init_record_ao (struct aoRecord *pao)
{	
	
	// Allocate memory for each channel
	for (int i = 0; i < 4; i++) {
		if (channels[i] == NULL) {
			channels[i] = (struct ChannelConfigs*)malloc(sizeof(struct ChannelConfigs));
		}
		if (trigger_config == NULL) {
			trigger_config = (struct TriggerConfigs*)malloc(sizeof(struct TriggerConfigs));
		}
	}
	channels[0]->channel = CHANNEL_A;
	channels[1]->channel = CHANNEL_B;
	channels[2]->channel = CHANNEL_C;
	channels[3]->channel = CHANNEL_D;

    if (sample_configurations == NULL) {
        sample_configurations = (struct SampleConfigs*)malloc(sizeof(struct SampleConfigs));
    }	

    struct instio  *pinst;
	struct PicoscopeData *vdp;

    if (pao->out.type != INST_IO)
    {
        errlogPrintf("%s: INP field type should be INST_IO\n", pao->name);
        return(S_db_badField);
    }
    pao->dpvt = calloc(sizeof(struct PicoscopeData), 1);
    if (pao->dpvt == (void *)0)
    {
    	errlogPrintf("%s: Failed to allocated memory\n", pao->name);
    	return -1;
    }
  
    pinst = &(pao->out.value.instio);
    vdp = (struct PicoscopeData *)pao->dpvt;

    if (format_device_support_function(pinst->string, vdp->paramLabel) != 0)
		{
			printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
		}

	vdp->ioType = findAioType(isOutput, vdp->paramLabel, &(vdp->cmdPrefix));

	if (vdp->ioType == UNKNOWN_IOTYPE)
	{
    	errlogPrintf("%s: Invalid type: \"%s\"\n", pao->name, vdp->paramLabel);
    	return(S_db_badField);
	}

	pao->udf = FALSE;

	switch (vdp->ioType)	
    {	
		case DEVICE_TO_OPEN: 
			device_serial_number = (int8_t*)pao->name;
			break; 

		case SET_RESOLUTION: 
			resolution = (int)pao->val; 
			break;

		case SET_NUM_SAMPLES: 
			sample_configurations->num_samples = (int)pao->val; 
			break; 

		case SET_DOWN_SAMPLE_RATIO: 
			sample_configurations->down_sample_ratio = (int)pao->val; 
			break; 
		
		case SET_DOWN_SAMPLE_RATIO_MODE: 
			sample_configurations->down_sample_ratio_mode = (int)pao->val; 
			break; 

		case SET_TRIGGER_POSITION_RATIO:
			sample_configurations->trigger_position_ratio = (int)pao->val;
			break;

		case SET_NUM_DIVISIONS: 
			sample_configurations->timebase_configs.num_divisions = (int) pao->val; 

			// Initialize timebase feedback only information to 0. 
			sample_configurations->timebase_configs.timebase = 0; 
			sample_configurations->timebase_configs.sample_interval_secs = 0; 
			sample_configurations->timebase_configs.sample_rate = 0; 
			break; 

		case SET_TIME_PER_DIVISION_UNIT: 
			sample_configurations->timebase_configs.time_per_division_unit = (int) pao->val; 
			break;

		case SET_TIME_PER_DIVISION: 
			sample_configurations->timebase_configs.time_per_division = (int) pao->val; 
			break; 

		case OPEN_PICOSCOPE: 
			// On initialization open picoscope with default resolution. 
			result = open_picoscope(resolution, device_serial_number);
			if (result != 0) {
				printf("Error opening picoscope with serial number %s\n", device_serial_number);
				pao->val = 0; // Cannot connect to picoscope, set PV to OFF. 
			}
			break;
		
		// Following cases are specific to a channel
        case SET_COUPLING:	
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			channels[channel_index]->coupling = (int)pao->val;
			break;

		case SET_RANGE: 
		 	record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			channels[channel_index]->range = (int)pao->val;
			break;

		case SET_ANALOG_OFFSET: 
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			pAnalogOffestRecords[channel_index] = pao; 

			double max_analog_offset = 0; 
			double min_analog_offset = 0; 
			result = get_analog_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analog_offset, &min_analog_offset);
			
			pao->drvh = max_analog_offset; 
			pao->hopr = max_analog_offset;

			pao->drvl = min_analog_offset;
			pao->lopr = min_analog_offset; 
			break;

		case SET_BANDWIDTH: 
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	

			channels[channel_index]->bandwidth= (int)pao->val;
			break;

		case SET_CHANNEL_ON:	

			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 

			// On initalization, set all channels off. 
			result = set_channel_off((int)channels[channel_index]->channel);
			if (result != 0) {
				printf("Error setting channel %s off.\n", record_name);
			}
			break;

		case SET_TRIGGER_DIRECTION:
			trigger_config->thresholdDirection = (enum ThresholdDirection) pao->val;
			break;

		case SET_TRIGGER_CHANNEL:
			trigger_config->channel = (enum Channel) pao->val;
			 
			break;

		case SET_TRIGGER_MODE:
			trigger_config->thresholdMode = (enum ThresholdMode) pao->val;
			trigger_pao[0] = pao;
			break;

		case SET_TRIGGER_UPPER:
			trigger_config->thresholdUpper = (int16_t) pao->val;
			trigger_pao[1] = pao;
			break;

		case SET_TRIGGER_LOWER:
			trigger_config->thresholdLower = (int16_t) pao->val;
			trigger_pao[2] = pao;
			break;
		default:
            return 0;
    }

	return 2;
}



static long
write_ao (struct aoRecord *pao)
{	
	uint32_t timebase = 0; 
	double sample_interval = 0; 
	double sample_rate = 0; 
	int16_t channel_status = 0;
	struct PicoscopeData *vdp;
	int returnState = 0;

    vdp = (struct PicoscopeData *)pao->dpvt;

	switch (vdp->ioType)
    {		
		case SET_RESOLUTION: 
			resolution = (int)pao->val; 
			result = set_device_resolution(resolution); 
			if (result !=0) {
				printf("Error setting picoscope resolution.\n");
			}
			re_acquire_waveform(pao);

			break;
		
		case SET_TIME_PER_DIVISION_UNIT: 
			int16_t previous_time_per_division_unit = sample_configurations->timebase_configs.time_per_division_unit;
			sample_configurations->timebase_configs.time_per_division_unit = (int) pao->val; 

			result = get_valid_timebase_configs(
				sample_configurations->timebase_configs, 
				sample_configurations->num_samples,
				&sample_interval, 
				&timebase, 
				&sample_rate
			); 

			if (result != 0) {
				sample_configurations->timebase_configs.time_per_division_unit = previous_time_per_division_unit; 
				break; 
			}
			
			sample_configurations->timebase_configs.sample_interval_secs = sample_interval;
			sample_configurations->timebase_configs.timebase = timebase;
			sample_configurations->timebase_configs.sample_rate = sample_rate;  
			re_acquire_waveform(pao);

			break; 

		case SET_TIME_PER_DIVISION: 
			double previous_time_per_division = sample_configurations->timebase_configs.time_per_division; 
			sample_configurations->timebase_configs.time_per_division = (int) pao->val; 

			result = get_valid_timebase_configs(
				sample_configurations->timebase_configs, 
				sample_configurations->num_samples,
				&sample_interval, 
				&timebase, 
				&sample_rate
			); 
			
			if (result != 0) {
				sample_configurations->timebase_configs.time_per_division = previous_time_per_division; 
				break; 
			}

			sample_configurations->timebase_configs.sample_interval_secs = sample_interval;
			sample_configurations->timebase_configs.timebase = timebase;
			sample_configurations->timebase_configs.sample_rate = sample_rate;  
			re_acquire_waveform(pao);

			break; 

		case SET_NUM_DIVISIONS: 
			int16_t previous_num_divisions = sample_configurations->timebase_configs.num_divisions; 
			sample_configurations->timebase_configs.num_divisions = (int) pao->val; 

			result = get_valid_timebase_configs(
				sample_configurations->timebase_configs, 
				sample_configurations->num_samples,
				&sample_interval, 
				&timebase, 
				&sample_rate
			); 

			if (result != 0) {
				sample_configurations->timebase_configs.num_divisions = previous_num_divisions; 
				break; 
			}

			sample_configurations->timebase_configs.sample_interval_secs = sample_interval;
			sample_configurations->timebase_configs.timebase = timebase;
			sample_configurations->timebase_configs.sample_rate = sample_rate;  
			re_acquire_waveform(pao);

			break; 

		case SET_NUM_SAMPLES:

			uint64_t previous_num_samples = sample_configurations->num_samples; 
			sample_configurations->num_samples = (int) pao->val; 
 
 			result = get_valid_timebase_configs(
				sample_configurations->timebase_configs, 
				sample_configurations->num_samples,
				&sample_interval, 
				&timebase, 
				&sample_rate
			); 

			if (result != 0) {
				sample_configurations->num_samples = previous_num_samples; 
				break; 
			}

			sample_configurations->timebase_configs.sample_interval_secs = sample_interval;
			sample_configurations->timebase_configs.timebase = timebase;
			sample_configurations->timebase_configs.sample_rate = sample_rate;
			re_acquire_waveform(pao);

			break;  
			
		case SET_DOWN_SAMPLE_RATIO: 
			sample_configurations->down_sample_ratio = (int)pao->val; 
			re_acquire_waveform(pao);

			break; 
		
		case SET_DOWN_SAMPLE_RATIO_MODE: 
			sample_configurations->down_sample_ratio_mode = (int)pao->val;
			re_acquire_waveform(pao);

			break; 

		case SET_TRIGGER_POSITION_RATIO:
			sample_configurations->trigger_position_ratio = (float)pao->val;
			re_acquire_waveform(pao);

			break;  
			
		case OPEN_PICOSCOPE: 
			int pv_value = (int)pao->val; 
			
			if (pv_value == 1){
				result = open_picoscope(resolution, device_serial_number);
				if (result != 0) {
					printf("Error opening picoscope with serial number %s\n", device_serial_number);
				}
			} else {
				result = close_picoscope(); 
				if (result != 0) {
					printf("Error closing picoscope.\n");
				}
			}
			break;

       	// Following cases are specific to a channel
        case SET_COUPLING:	
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 

			dbProcess((struct dbCommon *)pAnalogOffestRecords[channel_index]); 	
	
			int16_t previous_coupling = channels[channel_index]->coupling; 
			channels[channel_index]->coupling = (int)pao->val;

			channel_status = get_channel_status(channels[channel_index]->channel); 
			if (channel_status == 1) {
				result = set_channel_on(channels[channel_index]);
				// If channel is not succesfully set on, return to previous value 
				if (result != 0) {
					printf("Error setting %s to %d.\n", record_name, (int) pao->val);
					channels[channel_index]->coupling = previous_coupling;
					printf("Resetting to previous coupling.\n");
				}
			}
			break;
			re_acquire_waveform(pao);


		case SET_RANGE:
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			

			int16_t previous_range = channels[channel_index]->range; 

			channels[channel_index]->range = (int)pao->val;

			dbProcess((struct dbCommon *)pAnalogOffestRecords[channel_index]); 	

			channel_status = get_channel_status(channels[channel_index]->channel); 
			if (channel_status == 1){
				result = set_channel_on(channels[channel_index]);
				// If channel is not succesfully set on, return to previous value 
				if (result != 0) {
					printf("Error setting %s to %d.\n", record_name, (int) pao->val);
					channels[channel_index]->range = previous_range;
					printf("Resetting to previous range.\n");
				}
			}
			re_acquire_waveform(pao);

			break;

		case SET_ANALOG_OFFSET: 
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			int16_t previous_analog_offset = channels[channel_index]->coupling;

			double max_analog_offset = 0; 
			double min_analog_offset = 0; 
			result = get_analog_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analog_offset, &min_analog_offset);
			
			pao->drvh = max_analog_offset; 
			pao->hopr = max_analog_offset;

			pao->drvl = min_analog_offset;
			pao->lopr = min_analog_offset; 

			channels[channel_index]->analog_offset = pao->val; 
			
			channel_status = get_channel_status(channels[channel_index]->channel); 
			if (channel_status == 1) {
				result = set_channel_on(channels[channel_index]);
				// If channel is not succesfully set on, return to previous value 
				if (result != 0) {
					printf("Error setting %s to %d.\n", record_name, (int) pao->val);
					channels[channel_index]->analog_offset = previous_analog_offset;
					printf("Resetting to previous analog offset.\n");
				}
			}
			re_acquire_waveform(pao);

			break;

		case SET_BANDWIDTH: 
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			int16_t previous_bandwidth = channels[channel_index]->bandwidth;

			channels[channel_index]->bandwidth= (int)pao->val;

			channel_status = get_channel_status(channels[channel_index]->channel); 
			if (channel_status == 1) {
				result = set_channel_on(channels[channel_index]);
				// If channel is not succesfully set on, return to previous value 
				if (result != 0) {
					printf("Error setting %s to %d.\n", record_name, (int) pao->val);
					channels[channel_index]->bandwidth = previous_bandwidth;
					printf("Resetting to previous bandwidth.\n");
				}
			}
			re_acquire_waveform(pao);

			break;

		case SET_CHANNEL_ON:	
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 

			pv_value = pao->val;

			// If PV value is 1 (ON) set channel on 
			if (pv_value == 1) { 
				result = set_channel_on(channels[channel_index]);
				if (result != 0) {
					printf("Error setting channel %s on.\n", record_name);
					pao->val = 0; 
				}
			} 
			else {
				result = set_channel_off((int)channels[channel_index]->channel);
				if (result != 0) {
					printf("Error setting channel %s off.\n", record_name);
					pao->val = 0; 
				}
			}	

			// Update timebase configs that are affected by the number of channels on. 
			result = get_valid_timebase_configs(
				sample_configurations->timebase_configs, 
				sample_configurations->num_samples,
				&sample_interval, 
				&timebase, 
				&sample_rate
			); 
			sample_configurations->timebase_configs.sample_interval_secs = sample_interval;
			sample_configurations->timebase_configs.timebase = timebase;
			sample_configurations->timebase_configs.sample_rate = sample_rate;  
			re_acquire_waveform(pao);

			break;

		case SET_TRIGGER_DIRECTION:
			trigger_config->thresholdDirection = (enum ThresholdDirection) pao->val;
			re_acquire_waveform(pao);

			break;

		case SET_TRIGGER_CHANNEL:
			trigger_config->channel = (enum Channel) pao->val;
			if (trigger_config->channel == TRIGGER_AUX)
			{
				for (size_t i = 0; i < 3; i++)
				{
					trigger_pao[i]->val = 0;
					dbProcess((struct dbCommon *)trigger_pao[i]);
				}
			}
			re_acquire_waveform(pao);
			break;

		case SET_TRIGGER_MODE:
			trigger_config->thresholdMode = (enum ThresholdMode) pao->val;
			re_acquire_waveform(pao);

			break;

		case SET_TRIGGER_UPPER:
			trigger_config->thresholdUpper = (int16_t) pao->val;
			re_acquire_waveform(pao);

			break;

		case SET_TRIGGER_LOWER:
			trigger_config->thresholdLower = (int16_t) pao->val;
			re_acquire_waveform(pao);

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
    }
	return 0;
}

/** 
 * Gets the channel from the record name formatted "OSCXXXX-XX:CH[A-B]:" and returns index of that channel 
 * from an array of ChannelConfigs. 
 * 
 * @param record_name PV name formated "OSCXXXX-XX:CH[A-B]:"
 * 		  channels Array of ChannelConfigs 
 * 
 * @returns Index of channel in the channels array if successful, otherwise returns -1 
 * */
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs* channels[]) {
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
        if (channels[i]->channel == channel) {
            return i;  // Return index if channel matches
        }
    }

    return -1;  // Channel not found
}

/****************************************************************************************
 * Stringin - read a data array of values
 ****************************************************************************************/

typedef long (*DEVSUPFUN_STRINGIN)(struct stringinRecord *);


static long init_record_stringin(struct stringinRecord *);
static long read_stringin(struct stringinRecord *);

struct
        {
			long         number;
			DEVSUPFUN_STRINGIN report;
			DEVSUPFUN_STRINGIN init;
			DEVSUPFUN_STRINGIN init_record;
			DEVSUPFUN_STRINGIN get_ioint_info;
			DEVSUPFUN_STRINGIN read;
			// long (*special_linconv)(struct stringinRecord *, int);
        } devPicoscopeStringin =
                {
                6,
                NULL,
                NULL,
                init_record_stringin,
                NULL,
                read_stringin,
                };

epicsExportAddress(dset, devPicoscopeStringin);



static long
init_record_stringin(struct stringinRecord * pstringin)
{
	struct instio  *pinst;
    struct PicoscopeData *vdp;

	if (pstringin->inp.type != INST_IO)
	{
		printf("%s: INP field type should be INST_IO\n", pstringin->name);
		return(S_db_badField);
	}
	pstringin->dpvt = calloc(sizeof(struct PicoscopeData), 1);
	if (pstringin->dpvt == (void *)0)
	        {
	        printf("%s: Failed to allocated memory\n", pstringin->name);
	        return -1;
	        }
	pinst = &(pstringin->inp.value.instio);
	vdp = (struct PicoscopeData *)pstringin->dpvt;

    if (format_device_support_function(pinst->string, vdp->paramLabel) != 0)
		{
			printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
		}
    vdp->ioType = findAioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));
	if (vdp->ioType == UNKNOWN_IOTYPE){
		// errlogPrintf("%s: Invalid type: \"@%s\"\n", pai->name, vdp->paramLabel);
		printf("%s: Invalid type: \"@%s\"\n", pstringin->name, vdp->paramLabel);
		return(S_db_badField);
	}
	
	pstringin->udf = FALSE;

	switch (vdp->ioType)
	{
		case GET_DEVICE_INFO:
			int8_t* device_info = (int8_t*)"No device detected";
			result = get_device_info(&device_info);
			
			if (result != 0){
				printf("Error getting device information.\n");
			} 
			memcpy(pstringin->val, device_info, strlen((char *)device_info) + 1);
			
			break;
			
		default:
			return 0;
	}

	return 0;
}

static long
read_stringin (struct stringinRecord *pstringin){

	struct PicoscopeData *vdp = (struct PicoscopeData *)pstringin->dpvt;

	switch (vdp->ioType)
	{
		case GET_DEVICE_INFO:
			int8_t* device_info = (int8_t*)"No device detected";
			result = get_device_info(&device_info);
			
			if (result != 0){
				printf("Error getting device information.\n");
			} 
			memcpy(pstringin->val, device_info, strlen((char *)device_info) + 1);
			
			break;
			
		default:
			return 0;
	}
	
	return 0;

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

int16_t* waveform[CHANNEL_NUM];
int16_t waveform_size_actual;
int16_t waveform_size_max;
struct waveformRecord* pRecordUpdateWaveform[CHANNEL_NUM];

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

    if (format_device_support_function(pinst->string, vdp->paramLabel) != 0)
    {
    	errlogPrintf("%s: Invalid format: \"@%s\"\n", pwaveform->name, pinst->string);
        return(S_db_badField);
    }
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
			pWaveformStart = pwaveform;
			break;

		case UPDATE_WAVEFORM:
			int channel_index = find_channel_index_from_record(pwaveform->name, channels); 
			pRecordUpdateWaveform[channel_index] = pwaveform;
			waveform[channel_index] = (int16_t*)malloc(sizeof(int16_t) * pwaveform->nelm);
			waveform_size_max = pwaveform->nelm;
			// printf("channel:%s, nelm:%d\n",pwaveform->name,(int)pwaveform->nelm);
			if (waveform[channel_index] == NULL) {
				// Handle memory allocation failure
				fprintf(stderr, "Memory allocation failed\n");
				return -1;
			}
    		memset(waveform[channel_index], 0, sizeof(int16_t) * pwaveform->nelm);
			break;

		case STOP_RETRIEVE_WAVEFORM:
			epics_acquisition_control_mutex = epicsMutexCreate();
			if (epics_acquisition_control_mutex == NULL) {
				fprintf(stderr, "Failed to create epics_acquisition_control_mutex mutex\n");
				return -1;
			}
			epics_acquisition_thread_mutex = epicsMutexCreate();
			if (epics_acquisition_thread_mutex == NULL) {
				fprintf(stderr, "Failed to create epics_acquisition_thread_mutex mutex\n");
				return -1;
			}
			epics_acquisition_pv_mutex = epicsMutexCreate();
			if (epics_acquisition_pv_mutex == NULL) {
				fprintf(stderr, "Failed to create epics_acquisition_pv_mutex mutex\n");
				return -1;
			}
			
			pWaveformStop = pwaveform;
			break;

		default:
			return 0;
	}

	return 0;
}

struct CaptureThreadData {
    struct SampleConfigs *sample_config;
    struct ChannelConfigs **channel_configs;
    struct TriggerConfigs *trigger_config;
};
void captureThreadFunc(void *arg) {
	epicsMutexLock(epics_acquisition_thread_mutex);
    struct CaptureThreadData *data = (struct CaptureThreadData *)arg;
	epicsThreadId id = epicsThreadGetIdSelf();
	printf("Start ID is %ld\n", id->tid);
    // Setup Picoscope
    int16_t status = setup_picoscope(waveform, data->channel_configs, data->sample_config, data->trigger_config);
    if (status != 0) {
        fprintf(stderr, "setup_picoscope Error with code: %d \n", status);
        goto cleanup;
    }

    epicsMutexLock(epics_acquisition_control_mutex);
    // dataAcquisitionControl ++;
    epicsMutexUnlock(epics_acquisition_control_mutex);
    while (1) {
        epicsMutexLock(epics_acquisition_control_mutex);
        if (dataAcquisitionControl!=1) {
            epicsMutexUnlock(epics_acquisition_control_mutex);
            break;
        }
	
		dataAcquisitionFinished = 0;
        epicsMutexUnlock(epics_acquisition_control_mutex);
        double time_indisposed_ms = 0;

        status = run_block_capture(data->sample_config, &time_indisposed_ms, &dataAcquisitionControl);

        if (status != 0) {
            epicsMutexLock(epics_acquisition_control_mutex);
			dataAcquisitionFinished = 1;
            epicsMutexUnlock(epics_acquisition_control_mutex);
            fprintf(stderr, "run_block_capture Error with code: %d \n", status);
            break;
        }

        epicsMutexLock(epics_acquisition_control_mutex);
        waveform_size_actual = data->sample_config->num_samples;
		dataAcquisitionFinished = 1;
        epicsMutexUnlock(epics_acquisition_control_mutex);

        // Process the UPDATE_WAVEFORM subroutine to update waveform
        for (size_t i = 0; i < CHANNEL_NUM; i++) {
            int channel_index = find_channel_index_from_record(pRecordUpdateWaveform[i]->name, channels);
            if (is_Channel_On(data->channel_configs[channel_index]->channel)) {
                dbProcess((struct dbCommon *)pRecordUpdateWaveform[i]);
            }
        }
    }

cleanup:
	printf("Cleanup ID is %ld\n", id->tid);

    free(data->sample_config);
    for (size_t i = 0; i < CHANNEL_NUM; i++) {
        free(data->channel_configs[i]);
    }
    free(data->channel_configs);
    free(data->trigger_config);
    free(data);

    epicsMutexLock(epics_acquisition_control_mutex);
    // dataAcquisitionControl --;
    epicsMutexUnlock(epics_acquisition_control_mutex);
	epicsMutexUnlock(epics_acquisition_thread_mutex);
}

static long
read_waveform(struct waveformRecord *pwaveform) {
    int16_t status;
    struct PicoscopeData *vdp = (struct PicoscopeData *)pwaveform->dpvt;

    switch (vdp->ioType) {
        case START_RETRIEVE_WAVEFORM:
			epicsMutexLock(epics_acquisition_pv_mutex);
			printf("Start Retrieving\n");
            epicsMutexLock(epics_acquisition_control_mutex);
            if (dataAcquisitionControl >= 1) {
                fprintf(stderr, "%d Capture thread already running\n", dataAcquisitionControl);
				printf("Capture thread already running\n");
                epicsMutexUnlock(epics_acquisition_control_mutex);
				epicsMutexUnlock(epics_acquisition_pv_mutex);
                return -1;
            }else if (dataAcquisitionControl<0)
			{
				printf("elif control:%d\n", dataAcquisitionControl);
            	dataAcquisitionControl ++;
				epicsMutexUnlock(epics_acquisition_control_mutex);
				epicsMutexUnlock(epics_acquisition_pv_mutex);
                return -1;
			}
			
			printf("control:%d\n", dataAcquisitionControl);
            dataAcquisitionControl ++;
            epicsMutexUnlock(epics_acquisition_control_mutex);

            // Create CaptureThreadData
			struct CaptureThreadData *data = malloc(sizeof(struct CaptureThreadData));
			data->sample_config = data ? malloc(sizeof(struct SampleConfigs)) : NULL;
			data->channel_configs = data ? malloc(CHANNEL_NUM * sizeof(struct ChannelConfigs *)) : NULL;
			data->trigger_config = data ? malloc(sizeof(struct TriggerConfigs)) : NULL;
			if (!data || !data->sample_config || !data->channel_configs || !data->trigger_config) {
				fprintf(stderr, "Memory allocation failed for CaptureThreadData or its members\n");
				free(data->trigger_config);
				free(data->channel_configs);
				free(data->sample_config);
				free(data);
				epicsMutexLock(epics_acquisition_control_mutex);
				dataAcquisitionControl --;
				epicsMutexUnlock(epics_acquisition_control_mutex);
				epicsMutexUnlock(epics_acquisition_pv_mutex);

				return -1;
			}

            for (size_t i = 0; i < CHANNEL_NUM; i++) {
                data->channel_configs[i] = malloc(sizeof(struct ChannelConfigs));
                if (!data->channel_configs[i]) {
                    fprintf(stderr, "channel_configs[%zu] Memory allocation failed\n", i);
                    for (size_t j = 0; j < i; j++) {
                        free(data->channel_configs[j]);
                    }
                    free(data->channel_configs);
                    free(data->sample_config);
                    free(data);
                    epicsMutexLock(epics_acquisition_control_mutex);
                    dataAcquisitionControl --;
                    epicsMutexUnlock(epics_acquisition_control_mutex);
					epicsMutexUnlock(epics_acquisition_pv_mutex);
                    return -1;
                }
                *data->channel_configs[i] = *channels[i];
            }
			*data->sample_config = *sample_configurations;
			*data->trigger_config = *trigger_config;

            // Ensure sample size doesn't exceed maximum
            if (data->sample_config->num_samples > waveform_size_max) {
                printf("Sample size (%ld) exceeds the maximum available size (%d) for Picoscope. Setting sample size to the maximum value.\n",
                       data->sample_config->num_samples, waveform_size_max);
                data->sample_config->num_samples = waveform_size_max;
            }

            // Create capture thread
            epicsThreadId capture_thread = epicsThreadCreate("captureThread", epicsThreadPriorityMedium,
                                                             0, (EPICSTHREADFUNC)captureThreadFunc, data);
            if (!capture_thread) {
                fprintf(stderr, "Failed to create capture thread\n");
                free(data->trigger_config);
                for (size_t i = 0; i < CHANNEL_NUM; i++) {
                    free(data->channel_configs[i]);
                }
                free(data->channel_configs);
                free(data->sample_config);
                free(data);
                epicsMutexLock(epics_acquisition_control_mutex);
                dataAcquisitionControl --;
                epicsMutexUnlock(epics_acquisition_control_mutex);
				epicsMutexUnlock(epics_acquisition_pv_mutex);
                return -1;
            }
			epicsMutexUnlock(epics_acquisition_pv_mutex);

            break;

        case UPDATE_WAVEFORM:
            int channel_index = find_channel_index_from_record(pwaveform->name, channels);
            epicsMutexLock(epics_acquisition_control_mutex);
            if (dataAcquisitionControl == 1) {
                memcpy(pwaveform->bptr, waveform[channel_index], waveform_size_actual * sizeof(int16_t));
                pwaveform->nord = waveform_size_actual;
            }
            epicsMutexUnlock(epics_acquisition_control_mutex);
            break;

        case STOP_RETRIEVE_WAVEFORM:
            epicsMutexLock(epics_acquisition_control_mutex);
            dataAcquisitionControl --;
            epicsMutexUnlock(epics_acquisition_control_mutex);
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
