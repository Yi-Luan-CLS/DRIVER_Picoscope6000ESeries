/*
 * ---------------------------------------------------------------------
 * File:
 *     devPicoscopeCommon.h
 * Description:
 *     Common utilities for EPICS device support of Picoscope PS6000A.
 *     Provides functions for enum handling, record parsing, logging, 
 *     and waveform re-acquisition.
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
#ifndef DEV_PICOSCOPE
#define DEV_PICOSCOPE

#include "drvPicoscope.h"
#include <waveformRecord.h>

typedef struct MultiBitBinaryEnums {
    char* zrst; int zrvl; 
    char* onst; int onvl; 
    char* twst; int twvl; 
    char* thst; int thvl; 
    char* frst; int frvl; 
    char* fvst; int fvvl; 
    char* sxst; int sxvl; 
    char* svst; int svvl; 
    char* eist; int eivl; 
    char* nist; int nivl; 
    char* test; int tevl; 
    char* elst; int elvl; 
    char* tvst; int tvvl;
    char* ttst; int ttvl; 
    char* ftst; int ftvl;
    char* ffst; int ffvl; 
} MultiBitBinaryEnums;

void update_enum_options(struct mbboRecord* pmbbo, struct mbbiRecord* pmbbi, MultiBitBinaryEnums options);
int convertPicoscopeParams(char *string, char *paramName, char *serialNum);
void log_message(struct PS6000AModule* mp, char pv_name[], char error_message[], uint32_t status_code);
void re_acquire_waveform(struct PS6000AModule *mp);
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs channel_configs[NUM_CHANNELS]);

#endif