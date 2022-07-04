#ifndef CONTROLLO_DIGITALE_H_INCLUDED
#define CONTROLLO_DIGITALE_H_INCLUDED

#include <stdint.h>

void     controllo_digitale_init(void);
uint8_t  controllo_digitale_get_signal_on(void);
uint16_t controllo_digitale_get_analog_speed(void);
int      controllo_digitale_get_perc_speed(void);

#endif
