#include <string.h>
#include <assert.h>
#include "easyconnect_interface.h"
#include "model.h"


void model_init(model_t *pmodel) {
    assert(pmodel != NULL);
    static const uint8_t default_speed_steps[NUM_SPEED_STEPS] = {40, 55, 70, 85, 100};

    pmodel->sem = xSemaphoreCreateMutexStatic(&pmodel->semaphore_buffer);

    pmodel->address       = EASYCONNECT_DEFAULT_MINION_ADDRESS;
    pmodel->serial_number = EASYCONNECT_DEFAULT_MINION_SERIAL_NUMBER;

    pmodel->motor_control_override = 0;
    pmodel->group                  = DEVICE_GROUP_FAN_SIPHONING;

    memcpy(pmodel->speed_steps, default_speed_steps, sizeof(pmodel->speed_steps));
    pmodel->speed_step = 0;
}


void model_check_values(model_t *pmodel) {
    assert(pmodel != NULL);

    uint8_t group = model_get_group(pmodel);
    if (group != DEVICE_GROUP_FAN_SIPHONING && group != DEVICE_GROUP_FAN_IMMISSION) {
        model_set_group(pmodel, DEVICE_GROUP_FAN_SIPHONING);
    }
}


uint8_t model_get_configured_speed_step(model_t *pmodel, size_t step) {
    assert(pmodel != NULL);
    assert(step < NUM_SPEED_STEPS);

    return pmodel->speed_steps[step];
}


uint8_t model_get_current_speed(model_t *pmodel) {
    assert(pmodel != NULL);
    return model_get_configured_speed_step(pmodel, model_get_speed_step(pmodel));
}