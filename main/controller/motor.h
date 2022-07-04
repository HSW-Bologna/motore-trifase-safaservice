#ifndef MOTOR_H_INCLUDED
#define MOTOR_H_INCLUDED


#include <stdint.h>


void    motor_init(void);
void    motor_set_speed(uint8_t percentage);
void    motor_turn_off(void);
uint8_t motor_get_state(void);
void    motor_control(uint8_t enable, uint8_t percentage);


#endif