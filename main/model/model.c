#include <string.h>
#include <assert.h>
#include "easyconnect_interface.h"
#include "model.h"
#include "config/app_config.h"


static uint8_t valid_mode(uint16_t mode);


void model_init(model_t *pmodel) {
    assert(pmodel != NULL);
    pmodel->sem = xSemaphoreCreateMutexStatic(&pmodel->semaphore_buffer);

    pmodel->address       = EASYCONNECT_DEFAULT_MINION_ADDRESS;
    pmodel->serial_number = EASYCONNECT_DEFAULT_MINION_SERIAL_NUMBER;

    pmodel->class = EASYCONNECT_DEFAULT_DEVICE_CLASS;

    pmodel->motor_active      = 0;
    pmodel->speed_percentage  = 0;
    pmodel->missing_heartbeat = 0;

    memset(pmodel->safety_message, 0, sizeof(pmodel->safety_message));
}


void model_check_values(model_t *pmodel) {
    assert(pmodel != NULL);

    uint16_t class = model_get_class(pmodel);
    if (class != CLASS(DEVICE_MODE_FAN, DEVICE_GROUP_1) && class != CLASS(DEVICE_MODE_FAN, DEVICE_GROUP_2)) {
        model_set_class(pmodel, EASYCONNECT_DEFAULT_DEVICE_CLASS, NULL);
    }
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


void model_get_safety_message(void *args, char *string) {
    model_t *pmodel = args;
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    strcpy(string, pmodel->safety_message);
    xSemaphoreGive(pmodel->sem);
}


void model_set_safety_message(model_t *pmodel, const char *string) {
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    snprintf(pmodel->safety_message, sizeof(pmodel->safety_message), "%s", string);
    xSemaphoreGive(pmodel->sem);
}



static uint8_t valid_mode(uint16_t mode) {
    switch (mode) {
        case DEVICE_MODE_FAN:
            return 1;
        default:
            return 0;
    }
}