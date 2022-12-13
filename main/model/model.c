#include <string.h>
#include <assert.h>
#include "easyconnect_interface.h"
#include "model.h"
#include "config/app_config.h"


static uint8_t valid_mode(uint16_t mode);


void model_init(model_t *pmodel) {
    assert(pmodel != NULL);
    static const uint8_t default_speed_steps[NUM_SPEED_STEPS] = {10, 25, 40, 55, 100};

    pmodel->sem = xSemaphoreCreateMutexStatic(&pmodel->semaphore_buffer);

    pmodel->address       = EASYCONNECT_DEFAULT_MINION_ADDRESS;
    pmodel->serial_number = EASYCONNECT_DEFAULT_MINION_SERIAL_NUMBER;

    pmodel->motor_control_override = 0;
    pmodel->class                  = EASYCONNECT_DEFAULT_DEVICE_CLASS;

    memcpy(pmodel->speed_steps, default_speed_steps, sizeof(pmodel->speed_steps));
    pmodel->speed_step = 0;
}


void model_check_values(model_t *pmodel) {
    assert(pmodel != NULL);

    uint16_t class = model_get_class(pmodel);
    if (class != CLASS(DEVICE_MODE_SIPHONING_FAN, DEVICE_GROUP_1) &&
        class != CLASS(DEVICE_MODE_IMMISSION_FAN, DEVICE_GROUP_1)) {
        model_set_class(pmodel, EASYCONNECT_DEFAULT_DEVICE_CLASS, NULL);
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


uint16_t model_get_class(void *arg) {
    assert(arg != NULL);
    model_t *pmodel = arg;

    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    uint16_t result = (pmodel->class & CLASS_CONFIGURABLE_MASK) | (APP_CONFIG_HARDWARE_MODEL << 12);
    xSemaphoreGive(pmodel->sem);

    return result;
}


int model_set_class(void *arg, uint16_t class, uint16_t *out_class) {
    assert(arg != NULL);
    model_t *pmodel = arg;

    uint16_t corrected = class & CLASS_CONFIGURABLE_MASK;
    uint16_t mode      = CLASS_GET_MODE(corrected | (APP_CONFIG_HARDWARE_MODEL << 12));

    if (valid_mode(mode)) {
        if (out_class != NULL) {
            *out_class = corrected;
        }
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);
        pmodel->class = corrected;
        xSemaphoreGive(pmodel->sem);
        return 0;
    } else {
        return -1;
    }
}


static uint8_t valid_mode(uint16_t mode) {
    switch (mode) {
        case DEVICE_MODE_IMMISSION_FAN:
        case DEVICE_MODE_SIPHONING_FAN:
            return 1;
        default:
            return 0;
    }
}