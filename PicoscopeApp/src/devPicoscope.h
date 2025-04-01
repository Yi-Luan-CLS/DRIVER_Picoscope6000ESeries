#include "drvPicoscope.h"
#include <waveformRecord.h>

#ifndef DEV_PICOSCOPE
#define DEV_PICOSCOPE
int format_device_support_function(char *string, char *paramName, char *serialNum);
void log_message(struct PS6000AModule* mp, char pv_name[], char error_message[], uint32_t status_code);
void re_acquire_waveform(struct PS6000AModule *mp);
int find_channel_index_from_record(const char* record_name, struct ChannelConfigs channel_configs[CHANNEL_NUM]);
#endif