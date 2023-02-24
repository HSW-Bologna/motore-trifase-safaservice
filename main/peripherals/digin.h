#ifndef DIGIN_H_INCLUDED
#define DIGIN_H_INCLUDED

#include "hal/gpio_types.h"
#include <string.h>
#include <stdint.h>

typedef enum {
    DIGIN_SAFETY = 0,
} digin_t;

void         digin_init(void);
int          digin_get(digin_t digin);
int          digin_take_reading(void);
unsigned int digin_get_inputs(void);
uint8_t      digin_is_value_ready(void);

#endif