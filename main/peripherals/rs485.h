#ifndef RS485_H_INCLUDED
#define RS485_H_INCLUDED


#include <stdint.h>
#include <stdlib.h>


void rs485_init(void);
int  rs485_read(uint8_t *buffer, size_t len);
int  rs485_write(uint8_t *buffer, size_t len);
void rs485_flush(void);


#endif