#include "drvPicoscope.h"
#include <waveformRecord.h>

#ifndef DEV_PICOSCOPE
#define DEV_PICOSCOPE

typedef struct MultiBitBinaryEnums {
    char* zrst; int zrvl; 
    char* onst; int onvl; 
    char* twst; int twvl; 
} MultiBitBinaryEnums;

void update_enum_options(struct mbboRecord* pmbbo, struct mbbiRecord* pmbbi, MultiBitBinaryEnums options);
int convertPicoscopeParams(char *string, char *paramName, char *serialNum);
void log_message(struct PS6000AModule* mp, char pv_name[], char error_message[], uint32_t status_code);
void re_acquire_waveform(struct PS6000AModule *mp);
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs channel_configs[NUM_CHANNELS]);

#endif