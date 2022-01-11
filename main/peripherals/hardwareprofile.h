#ifndef HARDWAREPROFILE_H_INCLUDED
#define HARDWAREPROFILE_H_INCLUDED

#include <driver/gpio.h>

/*
 * Definizioni dei pin da utilizzare
 */

#define IO_LED_RUN GPIO_NUM_8

#define IO_ZCROS GPIO_NUM_6
#define IO_L1    GPIO_NUM_0
#define IO_L2    GPIO_NUM_1
#define IO_L3    GPIO_NUM_10
#define IO_VEL_ON GPIO_NUM_5
#define ANALOG_SPEED ADC1_CHANNEL_4
#define MB_UART_TXD GPIO_NUM_21
#define MB_UART_RXD GPIO_NUM_20
#define MB_DERE     GPIO_NUM_3

#endif