/*
 * ---------------------------------------------------------------------
 * File:
 *     devPicoscopeStringio.c
 * Description:
 *     Device support for EPICS stringin records for Picoscope PS6000A 
 *     module. Provides access to device information such as model name,
 *     and serial number.
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbAccess.h>
#include <epicsExport.h>
#include <errlog.h>

#include <stringinRecord.h>

#include "devPicoscopeCommon.h"

enum ioType
{
    UNKNOWN_IOTYPE, // default case, must be 0 
    GET_DEVICE_INFO,
};

enum ioFlag
{
    isOutput = 0,
    isInput = 1
};

static struct stringioType
{
    char *label;
    enum ioFlag flag;
    enum ioType ioType;
    char *cmdp;
} StringioType[] =
    {
        {"get_device_info", isInput, GET_DEVICE_INFO, "" }
    };

#define STRINGIO_TYPE_SIZE    (sizeof (StringioType) / sizeof (struct stringioType))

struct PicoscopeStringioData
{
    char serial_num[10];
    enum ioType ioType;
    char *cmdPrefix;
    char paramLabel[32];
    int paramValid;
    struct PS6000AModule* mp;
};

static enum ioType findStringioType(enum ioFlag ioFlag, char *param, char **cmdString)
{
    unsigned int i;

    for (i = 0; i < STRINGIO_TYPE_SIZE; i ++)
        if (strcmp(param, StringioType[i].label) == 0  &&  StringioType[i].flag == ioFlag)
            {
            *cmdString = StringioType[i].cmdp;
            return StringioType[i].ioType;
            }

    return UNKNOWN_IOTYPE;
}


/****************************************************************************************
 *  Stringin - read a data array of values
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


static long init_record_stringin(struct stringinRecord * pstringin)
{
    struct instio  *pinst;
    struct PicoscopeStringioData *vdp;

    if (pstringin->inp.type != INST_IO)
    {
        printf("%s: INP field type should be INST_IO\n", pstringin->name);
        return(S_db_badField);
    }
    pstringin->dpvt = calloc(sizeof(struct PicoscopeStringioData), 1);
    if (pstringin->dpvt == (void *)0)
            {
            printf("%s: Failed to allocated memory\n", pstringin->name);
            return -1;
            }
    pinst = &(pstringin->inp.value.instio);
    vdp = (struct PicoscopeStringioData *)pstringin->dpvt;

    if (convertPicoscopeParams(pinst->string, vdp->paramLabel, vdp->serial_num) != 0)
        {
            printf("Error when getting function name: %s\n",vdp->paramLabel);
            return -1;
        }
    vdp->mp = PS6000AGetModule(vdp->serial_num);

    vdp->ioType = findStringioType(isInput, vdp->paramLabel, &(vdp->cmdPrefix));
    if (vdp->ioType == UNKNOWN_IOTYPE){
        errlogPrintf("%s: Invalid type: \"@%s\"\n", pstringin->name, vdp->paramLabel);
        return(S_db_badField);
    }
    
    pstringin->udf = FALSE;

    switch (vdp->ioType)
    {
        case GET_DEVICE_INFO:
            int8_t* device_info = (int8_t*)"No device detected";
            uint32_t result = get_device_info(vdp->mp, &device_info);
            
            memcpy(pstringin->val, device_info, strlen((char *)device_info) + 1);
            
            if (result != 0){
                printf("Error getting device info.\n");
            } 
            else {
                free(device_info); 
            } 
            break;
            
        default:
            return 0;
    }

    return 0;
}

static long read_stringin (struct stringinRecord *pstringin){

    struct PicoscopeStringioData *vdp = (struct PicoscopeStringioData *)pstringin->dpvt;

    switch (vdp->ioType)
    {
        case GET_DEVICE_INFO:
            int8_t* device_info = (int8_t*)"No device detected";
            uint32_t result = get_device_info(vdp->mp, &device_info);
            
            memcpy(pstringin->val, device_info, strlen((char *)device_info) + 1);
            
            if (result != 0){
                update_log_pvs(vdp->mp, "Error getting device information.", result);
            } 
            else {
                free(device_info); 
            }
            
            break;
            
        default:
            return 0;
    }
    
    return 0;

}    