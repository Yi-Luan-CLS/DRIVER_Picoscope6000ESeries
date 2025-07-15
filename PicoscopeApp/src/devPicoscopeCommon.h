#ifndef DEV_PICOSCOPE
#define DEV_PICOSCOPE

#include "drvPicoscope.h"
#include <waveformRecord.h>

#define LOG_MESSAGE_LENGTH 500

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
void re_acquire_waveform(struct PS6000AModule *mp);
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs channel_configs[NUM_CHANNELS]);
void update_log_pvs(struct PS6000AModule* mp, char error_message[], uint32_t status_code); 

#endif