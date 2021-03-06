#include <assert.h>
#include <stdio.h>
#include "esp_log.h"
#include "model/model.h"
#include "peripherals/storage.h"
#include "configuration.h"


#define ADDRESS_KEY    "indirizzo"
#define SERIAL_NUM_KEY "numeroseriale"
#define GROUP_KEY      "group"


void configuration_init(model_t *pmodel) {
    uint16_t value       = 0;
    uint8_t  uint8_value = 0;

    if (storage_load_uint16(&value, ADDRESS_KEY) == 0) {
        model_set_address(pmodel, value);
    }
    if (storage_load_uint16(&value, SERIAL_NUM_KEY) == 0) {
        model_set_serial_number(pmodel, value);
    }
    if (storage_load_uint8(&uint8_value, GROUP_KEY) == 0) {
        model_set_group(pmodel, uint8_value);
    }
}


void configuration_save_serial_number(void *args, uint16_t value) {
    storage_save_uint16(&value, SERIAL_NUM_KEY);
    model_set_serial_number(args, value);
}


void configuration_save_address(void *args, uint16_t value) {
    storage_save_uint16(&value, ADDRESS_KEY);
    model_set_address(args, value);
}


void configuration_save_group(void *args, uint16_t value) {
    // This device has a fixed device model
    uint8_t group = value & 0xFF;
    storage_save_uint8(&group, GROUP_KEY);
    model_set_group(args, value);
}
