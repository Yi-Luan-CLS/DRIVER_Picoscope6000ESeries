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

enum ioType
	{
	UNKNOWN_IOTYPE, // default case, must be 0 
	OPEN_PICOSCOPE,
	GET_DEVICE_STATUS,
	SET_RESOLUTION,
	GET_RESOLUTION,
	SET_NUM_SAMPLES,
	GET_NUM_SAMPLES,
	GET_TIMEBASE,
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
	SET_ANALOGUE_OFFSET,
	GET_ANALOGUE_OFFSET,
	GET_MAX_ANALOGUE_OFFSET,
	GET_MIN_ANALOGUE_OFFSET,
	SET_BANDWIDTH, 
	GET_BANDWIDTH,
	RETRIEVE_WAVEFORM,
	SET_SAMPLE_INTERVAL, 
	GET_SAMPLE_INTERVAL,
	DEVICE_TO_OPEN
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
		{"set_analogue_offset", isOutput, SET_ANALOGUE_OFFSET, ""},
		{"get_analogue_offset", isInput, GET_ANALOGUE_OFFSET, ""},
		{"get_max_analogue_offset", isInput, GET_MAX_ANALOGUE_OFFSET, ""},
		{"get_min_analogue_offset", isInput, GET_MIN_ANALOGUE_OFFSET, ""},
		{"set_bandwidth", isOutput, SET_BANDWIDTH, "" }, 
		{"get_bandwidth", isInput, GET_BANDWIDTH, "" }, 
		{"retrieve_waveform", isInput, RETRIEVE_WAVEFORM, "" },
		{"set_sample_interval", isOutput, SET_SAMPLE_INTERVAL, "" },
		{"get_sample_interval", isInput, GET_SAMPLE_INTERVAL, "" },
		{"device_to_open", isOutput, DEVICE_TO_OPEN, ""}
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

int
format_device_support_function(char *string, char *paramName)
{
        if (sscanf(string, "L:%s",paramName) != 1)
                return -1;
        return 0;
}


struct ChannelConfigs* channels[4] = {NULL}; // List of Picoscope channels and their configurations

struct SampleConfigs* sample_configurations = NULL; // Configurations for data capture

char* record_name; 
int channel_index; 

double max_analogue_offset; 
double min_analogue_offset; 

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

			int16_t pv_enum_val = translate_resolution(resolution);

			pai->val = pv_enum_val;  
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

		case GET_ANALOGUE_OFFSET: 
			record_name = pai->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			pai->val = channels[channel_index]->analogue_offset; 
			break; 

		case GET_MAX_ANALOGUE_OFFSET: 
			record_name = pai->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	

			result = get_analogue_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analogue_offset, &min_analogue_offset);

			pai->val = max_analogue_offset; 
			break;

		case GET_MIN_ANALOGUE_OFFSET: 
			record_name = pai->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	

			result = get_analogue_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analogue_offset, &min_analogue_offset);

			pai->val = min_analogue_offset; 
			break;

		// Data configuration fbk 
		case GET_NUM_SAMPLES: 
			pai->val = sample_configurations->num_samples; 
			break; 

		case GET_DOWN_SAMPLE_RATIO: 
			pai->val = sample_configurations->down_sample_ratio; 
			break; 

		case GET_DOWN_SAMPLE_RATIO_MODE: 
			pai->val = translate_down_sample_ratio_mode(sample_configurations->down_sample_ratio_mode);
			break;

		case GET_TRIGGER_POSITION_RATIO: 
			pai->val = sample_configurations->trigger_position_ratio;
			break; 
			
		case GET_TIMEBASE: 
			pai->val = sample_configurations->timebase; 
			break; 
		
		case GET_SAMPLE_INTERVAL: 
			pai->val = sample_configurations->time_interval_secs; 
			break; 

		default:
			return 2;

	}
	return 2;

}	

#define PICO_DR_8BIT   0
#define PICO_DR_10BIT  1
#define PICO_DR_12BIT  2

/**
 * Match device resolution values from the Picoscope API to the corresponding 
 * value in the EPICS mbbo record $(OSC):resolution:fbk. 
 * 
 * @param mode The mode value as defined by the Picoscope API.
 * 
 * @return The value of the mode in the mbbi fbk PV, or -1 if mode does not exist.
 */
int16_t translate_resolution(int mode){
	if (mode == 0) {
		return PICO_DR_8BIT;
	}
	if (mode == 10){
		return PICO_DR_10BIT;
	}	
	if (mode == 1) {
		return PICO_DR_12BIT;
	}	
	return -1;
}


#define AGGREGATE      0
#define DECIMATE       1
#define AVERAGE        2
#define TRIG_DATA      3
#define TRIGGER        4
#define RAW            5

/**
 * Match downsample ratio mode values from the Picoscope API to the corresponding 
 * value in the EPICS mbbo record $(OSC):down_sample_ratio_mode:fbk. 
 * 
 * @param mode The mode value as defined by the Picoscope API
 * 
 * @return The value of the mode in the mbbi fbk PV, or -1 if mode does not exist.
 */
int16_t translate_down_sample_ratio_mode(int mode){
	if (mode == 1) {
		return AGGREGATE;
	}
	if (mode == 2){
		return DECIMATE;
	}	
	if (mode == 4) {
		return AVERAGE;
	}	
	if (mode & 268435456) {
		return TRIG_DATA;
	}
	if (mode & 1073741824) {
		return TRIGGER;
	}	
	if (mode == -2147483648) {
		return RAW;
	}
	return -1;
}

/****************************************************************************************
 * AO Record
 ****************************************************************************************/

#include <aoRecord.h>

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

static long
init_record_ao (struct aoRecord *pao)
{	
	
	// Allocate memory for each channel
	for (int i = 0; i < 4; i++) {
		if (channels[i] == NULL) {
			channels[i] = (struct ChannelConfigs*)malloc(sizeof(struct ChannelConfigs));
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
			sample_configurations->trigger_position_ratio = (float)pao->val;
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

		case SET_ANALOGUE_OFFSET: 
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			double max_analogue_offset = 0; 
			double min_analogue_offset = 0; 
			result = get_analogue_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analogue_offset, &min_analogue_offset);

			// If PV val is outside of the analogue offset limits, use the limit instead. 
			if (pao->val > max_analogue_offset) {
				channels[channel_index]->analogue_offset = max_analogue_offset;
			}
			else if (pao->val < min_analogue_offset){ 
				channels[channel_index]->analogue_offset = min_analogue_offset; 
			} 
			else {
				channels[channel_index]->analogue_offset = pao->val;
			}
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

        default:
            return 0;
    }

	return 2;
}

static long
write_ao (struct aoRecord *pao)
{	
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
			break;
		
		case SET_SAMPLE_INTERVAL: 
			uint32_t timebase; 
			double available_time_interval; 
			double requested_time_interval = pao->val;

			result = set_sample_interval(requested_time_interval, &timebase, &available_time_interval);
			if (result != 0) {
				printf("Error setting picoscope time interval.\n");
				break;
			}
			// Add returned values to sample configurations for next waveform acquired. 
			sample_configurations->time_interval_secs = available_time_interval; 
			sample_configurations->timebase = timebase;
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
			sample_configurations->trigger_position_ratio = (float)pao->val;
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

			int16_t previous_coupling = channels[channel_index]->coupling; 

			channels[channel_index]->coupling = (int)pao->val;

			result = set_channel_on(channels[channel_index]);
			// If channel is not succesfully set on, return to previous value 
			if (result != 0) {
				printf("Error setting %s to %d.\n", record_name, (int) pao->val);
				channels[channel_index]->coupling = previous_coupling;
				printf("Resetting to previous coupling.\n");
			}
			break;

		case SET_RANGE:
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			int16_t previous_range = channels[channel_index]->range; 

			channels[channel_index]->range = (int)pao->val;

			result = set_channel_on(channels[channel_index]);
			// If channel is not succesfully set on, return to previous value 
			if (result != 0) {
				printf("Error setting %s to %d.\n", record_name, (int) pao->val);
				channels[channel_index]->range = previous_range;
				printf("Resetting to previous range.\n");
			}
			break;

		case SET_ANALOGUE_OFFSET: 

			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	

			int16_t previous_analog_offset = channels[channel_index]->coupling;

			double max_analogue_offset = 0; 
			double min_analogue_offset = 0; 
			result = get_analogue_offset_limits(channels[channel_index]->range, channels[channel_index]->coupling, &max_analogue_offset, &min_analogue_offset);
			
			// If PV val is outside of the analogue offset limits, use the limit instead. 
			if (pao->val > max_analogue_offset) {
				channels[channel_index]->analogue_offset = max_analogue_offset;
			}
			else if (pao->val < min_analogue_offset){ 
				channels[channel_index]->analogue_offset = min_analogue_offset; 
			} 
			else {
				channels[channel_index]->analogue_offset = pao->val;
			}

			result = set_channel_on(channels[channel_index]);

			// If channel is not succesfully set on, return to previous value 
			if (result != 0) {
				printf("Error setting %s to %d.\n", record_name, (int) pao->val);
				channels[channel_index]->analogue_offset = previous_analog_offset;
				printf("Resetting to previous analog offset.\n");
			}
			break;

		case SET_BANDWIDTH: 
			record_name = pao->name;
			channel_index = find_channel_index_from_record(record_name, channels); 	
			
			int16_t previous_bandwidth = channels[channel_index]->bandwidth;

			channels[channel_index]->bandwidth= (int)pao->val;

			result = set_channel_on(channels[channel_index]);
			
			// If channel is not succesfully set on, return to previous value 
			if (result != 0) {
				printf("Error setting %s to %d.\n", record_name, (int) pao->val);
				channels[channel_index]->bandwidth = previous_bandwidth;
				printf("Resetting to previous bandwidth.\n");
			}
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

epicsMutexId epics_mutex = NULL;
int16_t* waveform = NULL;

static long init_record_waveform(struct waveformRecord * pwaveform)
{
	struct instio  *pinst;
	struct PicoscopeData *vdp;

	epics_mutex = epicsMutexCreate();
	if (!epics_mutex) {
		printf("Failed to create EPICS mutex\n");
		return -1;
	}
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

    waveform = (int16_t*)malloc(sizeof(int16_t) * pwaveform->nelm);

	return 0;
}


static long
read_waveform (struct waveformRecord *pwaveform){
    int16_t status;

	struct PicoscopeData *vdp = (struct PicoscopeData *)pwaveform->dpvt;
	switch (vdp->ioType)
    {
		case RETRIEVE_WAVEFORM:	
		    if (epicsMutexLock(epics_mutex) == epicsMutexLockTimeout) {
				// Handle error: mutex lock failed
				fprintf(stderr, "Failed to lock mutex\n");
				return -1;
			}
			record_name = pwaveform->name; 
			channel_index = find_channel_index_from_record(record_name, channels); 

			// Make local copies of configurations, make sure the PV changes when processing won't cause error 
			struct ChannelConfigs* channel_configurations_local;
			struct SampleConfigs* sample_configurations_local;
			channel_configurations_local = (struct ChannelConfigs*)malloc(sizeof(struct ChannelConfigs));
			sample_configurations_local = (struct SampleConfigs*)malloc(sizeof(struct SampleConfigs));

			if (channel_configurations_local == NULL || sample_configurations_local == NULL || waveform == NULL) {
				fprintf(stderr, "Memory allocation failed\n");
				epicsMutexUnlock(epics_mutex);
				return -1;
			}
			memset(waveform, 0, sizeof(int16_t) * pwaveform->nelm);

	        // Copy channel configurations to the local structure
			channel_configurations_local->channel = channels[channel_index]->channel;
			channel_configurations_local->coupling = channels[channel_index]->coupling;
			channel_configurations_local->range = channels[channel_index]->range;
			channel_configurations_local->analogue_offset = channels[channel_index]->analogue_offset;
			channel_configurations_local->bandwidth = channels[channel_index]->bandwidth;

	        // Copy sample configurations to the local structure
			sample_configurations_local->timebase = sample_configurations->timebase;
			sample_configurations_local->num_samples = sample_configurations->num_samples;
			sample_configurations_local->trigger_position_ratio = sample_configurations->trigger_position_ratio;
			sample_configurations_local->down_sample_ratio = sample_configurations->down_sample_ratio;
			sample_configurations_local->down_sample_ratio_mode = sample_configurations->down_sample_ratio_mode;
			
			// Ensure the number of samples does not exceed the maximum allowed by the waveform buffer
			if(sample_configurations_local->num_samples > pwaveform->nelm){
				sample_configurations_local->num_samples = pwaveform->nelm;
				printf("Sample size exceeds the maximum available size (%d) for Picoscope. Setting sample size to the maximum value.\n", 
				MAX_SAMPLE_SIZE);
			}

        	// Retrieve the waveform data using the local configurations
			status = retrieve_waveform(channel_configurations_local, sample_configurations_local, waveform);
			if(status != 0){
				fprintf(stderr, "retrieve_waveform Error with code: %d \n",status);
				epicsMutexUnlock(epics_mutex);
				return -1;
			}
			if(waveform==NULL){
				fprintf(stderr, "waveform is NULL after retrieve\n");
				epicsMutexUnlock(epics_mutex);
				return -1;
			}

			// Set the number of elements read in the waveform record, 
			// num_samples will be the acutall read data size, from retrieve_waveform()
			pwaveform->nord = sample_configurations_local->num_samples;
			
	        // Copy the retrieved waveform data to the EPICS waveform record buffer
			memcpy(pwaveform->bptr, waveform, pwaveform->nord * sizeof(int16_t) );
			epicsMutexUnlock(epics_mutex);

			break;
       	default:
			if (recGblSetSevr(pwaveform, READ_ALARM, INVALID_ALARM)  &&  errVerbose
				&&  (pwaveform->stat != READ_ALARM  ||  pwaveform->sevr != INVALID_ALARM)){
					errlogPrintf("%s: Read Error\n", pwaveform->name);
				}
			return -1;
    }



	return 0;
}

