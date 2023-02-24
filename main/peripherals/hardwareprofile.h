#ifndef HARDWAREPROFILE_H_INCLUDED
#define HARDWAREPROFILE_H_INCLUDED

#include <driver/gpio.h>

/*
 * Definizioni dei pin da utilizzare
 */

#define IO_LED_RED   GPIO_NUM_8
#define IO_LED_GREEN GPIO_NUM_9

#define MB_UART_TXD GPIO_NUM_21
#define MB_UART_RXD GPIO_NUM_20
#define MB_DERE     GPIO_NUM_7
#define HAP_OUTPUT  GPIO_NUM_3
#define HAP_PWM     GPIO_NUM_2
#define HAP_INPUT   GPIO_NUM_10

#endif