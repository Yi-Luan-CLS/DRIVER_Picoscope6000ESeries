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

#include "picoscopeConfig.h"
#include "drvPicoscope.h"
#include "picoscopeConfig.h"

int16_t pico_status;
#define MAX_SAMPLE_SIZE 1073741696

enum ioType
	{
	UNKNOWN_IOTYPE, // default case, must be 0 
	OPEN_PICOSCOPE,
	SET_RESOLUTION,
  	GET_SERIAL_NUM,
	SET_CHANNEL_ON,
	SET_COUPLING,
	SET_RANGE, 
	RETRIEVE_WAVEFORM,
	SET_ANALOGUE_OFFSET,
	SET_BANDWIDTH
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
		{"set_resolution", isOutput, SET_RESOLUTION, ""},
	 	{"get_serial_num", isInput, GET_SERIAL_NUM, "" },
		{"set_channel_on", isOutput, SET_CHANNEL_ON, ""}, 
		{"set_coupling", isOutput, SET_COUPLING, "" },
		{"retrieve_waveform", isInput, RETRIEVE_WAVEFORM, "" },
		{"set_range", isOutput, SET_RANGE,   "" }, 
		{"set_analogue_offset", isOutput, SET_ANALOGUE_OFFSET, ""},
		{"set_bandwidth", isOutput, SET_BANDWIDTH, "" }, 
    };

#define AIO_TYPE_SIZE    (sizeof (AioType) / sizeof (struct aioType))

struct PicoscopeData
	{
		enum ioType ioType;
	    char serial_num[100];
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


/****************************************************************************************
 * AI Record
 ****************************************************************************************/

typedef long (*DEVSUPFUN_AI)(struct aiRecord *);

int integerRatio(double value, long *numerator, long *denominator);

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
		case GET_SERIAL_NUM:
			// printf("case invoked\n");
			// pai->val = get_serial_num();
			return 1;
			break;
		default:
			return 2;

		return 0;
	}
}	
	// static long
	// special_linconv_ai(struct aiRecord *pai, int after)
	// {
	// 		if (!after)
	// 				return 0;
	// 		/* set linear conversion slope*/
	// 		/* pai->eslo = (pai->eguf - pai->egul) / 65535.0; */
	// 		return 0;
	// }

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

struct ChannelConfigs* channel_b = NULL;

int16_t resolution = 0;

static long
init_record_ao (struct aoRecord *pao)
{	
	if (channel_b == NULL)
	{
		channel_b = malloc(sizeof(struct ChannelConfigs));
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
		case SET_RESOLUTION: 
			resolution = (int)pao->val; 
			break;

		case OPEN_PICOSCOPE: 
			int pv_value = (int)pao->val; 

			if (pv_value == 1){
				pico_status = open_picoscope(resolution);
			} else {
				pico_status = close_picoscope(); 
			}
			break;

        case SET_COUPLING:	
			channel_b->coupling = (int)pao->val;
			break;

		case SET_RANGE: 
			channel_b->range = (int)pao->val;
			break;

		case SET_ANALOGUE_OFFSET: 
			channel_b->analogue_offset = pao->val;
			break;

		case SET_BANDWIDTH: 
			channel_b->bandwidth= (int)pao->val;
			break;

		case SET_CHANNEL_ON:	

			char* record_name = pao->name;
			enum enPicoChannel channel_name = record_name_to_pico_channel(record_name);
		    
			channel_b->channel = channel_name;
			// Get value of PV OSCXXXX-XX:CH[A-B]:ON:set 
			pv_value = pao->val;

			// If PV value is 1 (ON) set channel on 
			if (pv_value == 1) { 
				pico_status = set_channel_on(channel_b);
			} else {
				pico_status = set_channel_off(channel_b->channel);
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
			break;

		case OPEN_PICOSCOPE: 
			int pv_value = (int)pao->val; 

			if (pv_value == 1){
				pico_status = open_picoscope(resolution);
			} else {
				pico_status = close_picoscope(); 
			}
			break;

        case SET_COUPLING:	
			channel_b->coupling = (int)pao->val;
			break;

		case SET_RANGE: 
			channel_b->range = (int)pao->val;
			break;

		case SET_ANALOGUE_OFFSET: 
			channel_b->analogue_offset = pao->val;
			break;

		case SET_BANDWIDTH: 
			channel_b->bandwidth= (int)pao->val;
			break;

		case SET_CHANNEL_ON:	

			char* record_name = pao->name;
			enum enPicoChannel channel_name = record_name_to_pico_channel(record_name);
		    
			channel_b->channel = channel_name;

			// Get value of PV OSCXXXX-XX:CH[A-B]:ON:set 
			pv_value = pao->val;

			// If PV value is 1 (ON) set channel on 
			if (pv_value == 1) { 
				pico_status = set_channel_on(channel_b);
			}
			else {
				pico_status = set_channel_off(channel_b->channel);
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

/** Get the channel from the PV format OSCXXXX-XX:CH[A-B]:*/
enum enPicoChannel record_name_to_pico_channel(const char* record_name) {
	char channel_str[4]; 
	sscanf(record_name, "%*[^:]:%4[^:]", channel_str); // Strip out CH[A-B] of PV name

    if (strcmp(channel_str, "CHA") == 0) {
        return PICO_CHANNEL_A;
    }
    else if (strcmp(channel_str, "CHB") == 0) {
        return PICO_CHANNEL_B;
    }
    else if (strcmp(channel_str, "CHC") == 0) {
        return PICO_CHANNEL_C;
    }
    else if (strcmp(channel_str, "CHD") == 0) {
        return PICO_CHANNEL_D;
    }
    
    return -1;  
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

	return 0;
}

static long
read_stringin (struct stringinRecord *pstringin){

	struct PicoscopeData *vdp = (struct PicoscopeData *)pstringin->dpvt;

	switch (vdp->ioType)
	{
		case GET_SERIAL_NUM:
			int8_t* serial_num = NULL;
			pico_status = get_serial_num(&serial_num);

			if (pico_status != PICO_OK || serial_num == NULL){
				// printf("Failed to require serial_num. Error code: %d\n", pico_status);
				return 1;
			} else memcpy(pstringin->val, serial_num, strlen((char *)serial_num) + 1);
		
			break;
		default:
			return 2;

		return 0;
	}
}	
// 	static long
// 	special_linconv_ai(struct aiRecord *pai, int after)
// 	{
// 			if (!after)
// 					return 0;
// 			/* set linear conversion slope*/
// 			/* pai->eslo = (pai->eguf - pai->egul) / 65535.0; */
// 			return 0;
// }

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

	// if(isInitialised == 0){
	// 	if(dev_connect_picoscope()){
	// 		return 1;
	// 	}
	// 	isInitialised++;
	// }
	// printf("waveform init\n");

	return 0;
}

static long
read_waveform (struct waveformRecord *pwaveform){
    int returnState;

	struct PicoscopeData *vdp = (struct PicoscopeData *)pwaveform->dpvt;
	int sample_size = 1073741696;
	int16_t* waveform = NULL;
    if(sample_size > pwaveform->nelm){
		sample_size = pwaveform->nelm;
    	printf("Sample size exceeds the maximum available size (%d) for Picoscope. Setting sample size to the maximum value.\n", 
		 MAX_SAMPLE_SIZE);
	}
	switch (vdp->ioType)
    {
		case RETRIEVE_WAVEFORM:	
			char* record_name = pwaveform->name; 
			enum enPicoChannel channel = record_name_to_pico_channel(record_name);
			struct ChannelConfigs config = {
				.channel = 1,
				.coupling = 1,
				.range = 10,
				.analogue_offset = 0.0,
				.bandwidth = 0
			};
			pwaveform->nord = sample_size;
			retrieve_waveform(&config, &waveform, sample_size);
    		returnState = 0;
			break;
       	default:
            returnState = -1;
    }

	if (returnState < 0){
        if (recGblSetSevr(pwaveform, READ_ALARM, INVALID_ALARM)  &&  errVerbose
        	&&  (pwaveform->stat != READ_ALARM  ||  pwaveform->sevr != INVALID_ALARM)){
	        	errlogPrintf("%s: Read Error\n", pwaveform->name);
			}
        return 2;
	}
	if(waveform==NULL){
		printf("\nwaveform is NULL\n");
		return 0;
	}
	memcpy(pwaveform->bptr, waveform, sample_size * sizeof(int16_t) );
	free(waveform);

	return 0;
}

