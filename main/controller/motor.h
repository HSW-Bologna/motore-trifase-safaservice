#ifndef MOTOR_H_INCLUDED
#define MOTOR_H_INCLUDED


#include <stdint.h>
#include "model/model.h"


void motor_init(model_t *pmodel);
void motor_set_speed(model_t *pmodel, uint8_t percentage);
void motor_turn_off(model_t *pmodel);
void motor_turn_on(model_t *pmodel);
void motor_refresh(model_t *pmodel);


#endif