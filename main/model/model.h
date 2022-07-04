#ifndef MODEL_H_INCLUDED
#define MODEL_H_INCLUDED


#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


#define EASYCONNECT_DEFAULT_MINION_ADDRESS       1
#define EASYCONNECT_DEFAULT_MINION_SERIAL_NUMBER 2
#define EASYCONNECT_DEFAULT_MINION_MODEL         0x0101
#define EASYCONNECT_DEFAULT_FEEDBACK_LEVEL       0x0
#define EASYCONNECT_DEFAULT_ACTIVATE_ATTEMPTS    1
#define EASYCONNECT_DEFAULT_FEEDBACK_DELAY       4

#define NUM_SPEED_STEPS 5

#define GETTER_UNSAFE(name, field)                                                                                     \
    static inline __attribute__((always_inline)) typeof(((model_t *)0)->field) model_get_##name(model_t *pmodel) {     \
        assert(pmodel != NULL);                                                                                        \
        return pmodel->field;                                                                                          \
    }

#define SETTER_UNSAFE(name, field)                                                                                     \
    static inline                                                                                                      \
        __attribute__((always_inline)) void model_set_##name(model_t *pmodel, typeof(((model_t *)0)->field) value) {   \
        assert(pmodel != NULL);                                                                                        \
        pmodel->field = value;                                                                                         \
    }

#define GETTER(type, name, field)                                                                                      \
    static inline __attribute__((always_inline)) typeof(((model_t *)0)->field) model_get_##name(type *arg) {           \
        model_t *pmodel = arg;                                                                                         \
        assert(pmodel != NULL);                                                                                        \
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);                                                                    \
        typeof(((model_t *)0)->field) res = pmodel->field;                                                             \
        xSemaphoreGive(pmodel->sem);                                                                                   \
        return res;                                                                                                    \
    }

#define SETTER(type, name, field)                                                                                      \
    static inline                                                                                                      \
        __attribute__((always_inline)) void model_set_##name(type *arg, typeof(((model_t *)0)->field) value) {         \
        model_t *pmodel = arg;                                                                                         \
        assert(pmodel != NULL);                                                                                        \
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);                                                                    \
        pmodel->field = value;                                                                                         \
        xSemaphoreGive(pmodel->sem);                                                                                   \
    }

#define GETTER_GENERIC(name, field) GETTER(void, name, field)
#define SETTER_GENERIC(name, field) SETTER(void, name, field)

#define GETTER_MODEL(name, field) GETTER(model_t, name, field)
#define SETTER_MODEL(name, field) SETTER(model_t, name, field)

#define GETTERNSETTER_GENERIC(name, field)                                                                             \
    GETTER_GENERIC(name, field)                                                                                        \
    SETTER_GENERIC(name, field)

#define GETTERNSETTER(name, field)                                                                                     \
    GETTER_MODEL(name, field)                                                                                          \
    SETTER_MODEL(name, field)

#define GETTERNSETTER_UNSAFE(name, field)                                                                              \
    GETTER_UNSAFE(name, field)                                                                                         \
    SETTER_UNSAFE(name, field)



typedef struct {
    StaticSemaphore_t semaphore_buffer;
    SemaphoreHandle_t sem;

    uint16_t address;
    uint16_t serial_number;
    uint8_t  group;

    uint8_t motor_control_override;

    uint8_t speed_steps[NUM_SPEED_STEPS];
    size_t  speed_step;
} model_t;


void    model_init(model_t *model);
void    model_check_values(model_t *pmodel);
uint8_t model_get_configured_speed_step(model_t *pmodel, size_t step);
uint8_t model_get_current_speed(model_t *pmodel);

GETTERNSETTER_GENERIC(address, address);
GETTERNSETTER_GENERIC(serial_number, serial_number);
GETTERNSETTER_GENERIC(group, group);
GETTERNSETTER(speed_step, speed_step);
GETTERNSETTER_UNSAFE(motor_control_override, motor_control_override);

#endif