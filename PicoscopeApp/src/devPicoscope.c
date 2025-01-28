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

#include "errlog.h"

#include "drvPicoscope.h"


int16_t pico_status;

enum ioType
	{
	UNKNOWN_IOTYPE, // default case, must be 0 
    GET_SERIAL_NUM,
	SET_COUPLING,
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
	    { "get_serial_num",		isInput,	GET_SERIAL_NUM, "" },
		{ "set_coupling", 		isOutput, 	SET_COUPLING,   "" },
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
	int pico_status;
    if (pai->inp.type != INST_IO)
    {
        // errlogPrintf("%s: INP field type should be INST_IO\n", pai->name);
        return(S_db_badField);
    }
    pai->dpvt = calloc(sizeof(struct PicoscopeData), 1);
    if (pai->dpvt == (void *)0)
    {
    // errlogPrintf("%s: Failed to allocated memory\n", pai->name);
	   return -1;
    }

    pinst = &(pai->inp.value.instio);
    vdp = (struct PicoscopeData *)pai->dpvt;

    if (sscanf(pinst->string, "L:%s", vdp->paramLabel) != 1) 
	{
        return -1;
	}

	vdp->ioType = findAioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));

	if (vdp->ioType == UNKNOWN_IOTYPE)
	{
		// errlogPrintf("%s: Invalid type: \"@%s\"\n", pai->name, vdp->paramLabel);
		printf("%s: Invalid type: \"@%s\"\n", pai->name, vdp->paramLabel);
		return(S_db_badField);
	}

	pai->udf = FALSE;
	printf("Picoscope Connecting...");
	pico_status = connect_picoscope();
	if (pico_status != PICO_OK){
		printf("Failed to open PicoScope device. Error code: %d\n", pico_status);
		return 1;
	}
	printf("Picoscope Connected");
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

int glb_channel = 0; 
int glb_coupling = 1;
int glb_voltage = 2; 
int glb_bandwidth = 3; 

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

static long
init_record_ao (struct aoRecord *pao)
{
	printf("init ao\n");

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
		printf("%s\n", pinst->string);

        if (sscanf(pinst->string, "L:%s",vdp->paramLabel) != 1)
			{
				printf("%s\n",vdp->paramLabel);
                return -1;
			}

	vdp->ioType = findAioType(isOutput, vdp->paramLabel, &(vdp->cmdPrefix));

	if (vdp->ioType == UNKNOWN_IOTYPE)
		{
                errlogPrintf("%s: Invalid type: \"%s\"\n", pao->name, vdp->paramLabel);
                return(S_db_badField);
		}

	if(isInitialised == 0){
		pico_status = connect_picoscope();
		if (pico_status != PICO_OK){
			printf("Failed to open PicoScope device. Error code: %d\n", pico_status);
			return 1;
		}
		isInitialised++;
	}

	switch (vdp->ioType)
                {
        case SET_COUPLING:	
			printf("Set coupling\n");
			glb_coupling = (int)pao->val;
			set_coupling(glb_coupling);

        default:
                return 0;
                }

	pao->udf = FALSE;

	return 2;
}

static long
write_ao (struct aoRecord *pao)
{	
	printf("write ao\n");
	struct PicoscopeData *vdp;
	int returnState = 0;

    vdp = (struct PicoscopeData *)pao->dpvt;

	switch (vdp->ioType)
        {
        case SET_COUPLING:	
			printf("Set coupling\n");
			glb_coupling = (int)pao->val;
			set_coupling(glb_coupling);
			break;

        default:
				printf("%d", vdp->ioType);
                returnState = -1;
        }

	if (returnState < 0)
                {
                if (recGblSetSevr(pao, READ_ALARM, INVALID_ALARM)  &&  errVerbose
                    &&  (pao->stat != READ_ALARM  ||  pao->sevr != INVALID_ALARM))
                        //errlogPrintf("%s: Read Error\n", pao->name);
                return 2;
                }

	return 0;
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
	if(isInitialised == 0){
		pico_status = connect_picoscope();
		if (pico_status != PICO_OK){
			printf("Failed to open PicoScope device. Error code: %d\n", pico_status);
			return 1;
		}
		isInitialised++;
	}
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
        if (sscanf(pinst->string, "L:%s",vdp->paramLabel) != 1) 
		{
                return -1;
		}

        vdp->ioType = findAioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));
		if (vdp->ioType == UNKNOWN_IOTYPE)
			{
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
				printf("Failed to require serial_num. Error code: %d\n", pico_status);
				return 1;
			} else memcpy(pstringin->val, serial_num, strlen((char *)serial_num) + 1);
		

		default:
			return 2;

		return 0;
	}
}	

/****************************************************************************************
 * Waveform - read a data array of values
 ****************************************************************************************/

//#include <waveformRecord.h>
//
//typedef long (*DEVSUPFUN_WAVEFORM)(struct waveformRecord *);
//
//
//static long init_record_waveform(struct waveformRecord *);
//static long read_waveform(struct waveformRecord *);
//
//struct
//        {
//        long         number;
//        DEVSUPFUN_WAVEFORM report;
//        DEVSUPFUN_WAVEFORM init;
//        DEVSUPFUN_WAVEFORM init_record;
//        DEVSUPFUN_WAVEFORM get_ioint_info;
//        DEVSUPFUN_WAVEFORM write_lout;
//        } devPicoscopeWaveform =
//                {
//                6,
//                NULL,
//                NULL,
//                init_record_waveform,
//                NULL,
//                read_waveform,
//                };
//
//epicsExportAddress(dset, devPicoscopeWaveform);
//
//
//static long init_record_waveform(struct waveformRecord * pwaveform)
//{
//	struct instio  *pinst;
//    struct PicoscopeData *vdp;
//
//
//        if (pwaveform->inp.type != INST_IO)
//                {
//                errlogPrintf("%s: INP field type should be INST_IO\n", pwaveform->name);
//                return(S_db_badField);
//                }
//        pwaveform->dpvt = calloc(sizeof(struct PicoscopeData), 1);
//        if (pwaveform->dpvt == (void *)0)
//                {
//                errlogPrintf("%s: Failed to allocated memory\n", pwaveform->name);
//                return -1;
//                }
//
//        pinst = &(pwaveform->inp.value.instio);
//        vdp = (struct PicoscopeData *)pwaveform->dpvt;
//
//        if (convertUSBSpectrometersParam(pinst->string, vdp->paramLabel)  <  0)
//                {
//                errlogPrintf("%s: Invalid format: \"@%s\"\n", pwaveform->name, pinst->string);
//                return(S_db_badField);
//                }
//        vdp->ioType = findAioType(isOutput, vdp->paramLabel, &(vdp->cmdPrefix));
//
//        if (vdp->ioType == UNKNOWN_IOTYPE)
//                {
//                errlogPrintf("%s: Invalid type: \"@%s\"\n", pwaveform->name, vdp->paramLabel);
//                return(S_db_badField);
//                }
//	spectra = (double*)calloc( pwaveform->nelm + 1, sizeof (double));
//	vdp->spectra = spectra;
//
//	if(isInitialised == 0){
//		initAcquisition(glb_index,glb_channel,glb_integration_usec,glb_averages,glb_boxcar);
//		isInitialised++;
//	}
//
//	switch (vdp->ioType)
//                {
//        case AQUIRE_SPECTRUM:
//		printf("INIT AQUIRE_SPECTRUM Waveform\n");
//
//		break;
//	case GET_AXIS_INFO:
//		printf("INIT GET_AXIS_INFO Waveform\n");
//		getAxisInfo(glb_index, &pwaveform->nelm,spectra);
//		pwaveform->nord = pwaveform->nelm;
//
//		memcpy(pwaveform->bptr, spectra, pwaveform->nelm * sizeof (double) );
//                break;
//        default:
//                printf("default, no init done\n");
//                }
//
//        pwaveform->udf = FALSE;
//        return 0;
//
//}
//
//static long
//read_waveform (struct waveformRecord *pwaveform)
//{
//        int returnState;
//
//	double *spectra;
//
//        struct PicoscopeData *vdp = (struct PicoscopeData *)pwaveform->dpvt;
//
//	spectra = vdp->spectra;
//        switch (vdp->ioType)
//                {
//        case AQUIRE_SPECTRUM:
//
//        	debugprint("Doing Acquisition\n");
//                doAcquisition(glb_index,glb_channel,glb_integration_usec,glb_averages,glb_boxcar,glb_iterations, pwaveform->nelm, spectra);
//		/*for(int x = 0; x<pwaveform->nelm; x++){
//			debugprint("spectra[%d] = %f\n",x,spectra[x]);
//		}*/
//		pwaveform->nord = pwaveform->nelm;
//		returnState = 0;
//
//                break;
//	case GET_AXIS_INFO:
//		getAxisInfo(glb_index, &pwaveform->nelm,spectra);
//		pwaveform->nord = pwaveform->nelm;
//		returnState = 0;
//		break;
//        default:
//		debugprint("default\n");
//                returnState = -1;
//                }
//
//        if (returnState < 0)
//                {
//                if (recGblSetSevr(pwaveform, READ_ALARM, INVALID_ALARM)  &&  errVerbose
//                    &&  (pwaveform->stat != READ_ALARM  ||  pwaveform->sevr != INVALID_ALARM))
//                        errlogPrintf("%s: Read Error\n", pwaveform->name);
//                return 2;
//                }
//
//	memcpy(pwaveform->bptr, spectra, pwaveform->nelm * sizeof (double) );
//
//        return 0;
//}
//