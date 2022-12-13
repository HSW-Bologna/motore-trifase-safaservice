#include <assert.h>
#include <stdio.h>
#include "esp_log.h"
#include "model/model.h"
#include "peripherals/storage.h"
#include "configuration.h"


#define ADDRESS_KEY    "indirizzo"
#define SERIAL_NUM_KEY "numeroseriale"
#define CLASS_KEY      "CLASS"


void configuration_init(model_t *pmodel) {
    uint16_t value = 0;

    if (storage_load_uint16(&value, ADDRESS_KEY) == 0) {
        model_set_address(pmodel, value);
    }
    if (storage_load_uint16(&value, SERIAL_NUM_KEY) == 0) {
        model_set_serial_number(pmodel, value);
    }
    if (storage_load_uint16(&value, CLASS_KEY) == 0) {
        model_set_class(pmodel, value, NULL);
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


int configuration_save_class(void *args, uint16_t value) {
    uint16_t corrected;
    if (model_set_class(args, value, &corrected) == 0) {
        storage_save_uint16(&corrected, CLASS_KEY);
        return 0;
    } else {
        return -1;
    }
}