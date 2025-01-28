#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "ps6000aApi.h"
#include "PicoStatus.h"

int16_t handle = 0;
int8_t* serial_num = NULL;
int MAX_CONNECT_TRIES = 12;
int
connect_picoscope(){
    int16_t status;
    bool open = false;
    int tries = 0;
    while(!open){
        status = ps6000aOpenUnit(&handle, NULL, PICO_DR_12BIT);
        tries ++;
        if (status != PICO_OK && tries <= MAX_CONNECT_TRIES) {
            sleep(5);
        }else{  open = true;    }
    }
    return status;
}

int16_t
get_serial_num(int8_t** serial_num_return) {
    int16_t status;
    int16_t required_size;

    status = ps6000aGetUnitInfo(handle, NULL, 0, &required_size, PICO_BATCH_AND_SERIAL);
    if (status != PICO_OK) 
        return status;  

    if(serial_num == NULL)
        serial_num = malloc(required_size);
    memset(serial_num, 0, required_size);

    status = ps6000aGetUnitInfo(handle, serial_num, required_size, &required_size, PICO_BATCH_AND_SERIAL);

    *serial_num_return = serial_num;

    return status;  
}
