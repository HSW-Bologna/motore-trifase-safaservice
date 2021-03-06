#ifndef CONFIGURATION_H_INCLUDED
#define CONFIGURATION_H_INCLUDED


#include <stdint.h>
#include "model/model.h"


void configuration_init(model_t *pmodel);
void configuration_save_serial_number(void *args, uint16_t value);
void configuration_save_address(void *args, uint16_t value);
void configuration_save_group(void *args, uint16_t value);


#endif