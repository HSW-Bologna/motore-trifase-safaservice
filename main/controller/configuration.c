#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "esp_log.h"
#include "model/model.h"
#include "peripherals/storage.h"
#include "configuration.h"


#define ADDRESS_KEY        "indirizzo"
#define SERIAL_NUM_KEY     "numeroseriale"
#define CLASS_KEY          "CLASS"
#define SAFETY_MESSAGE_KEY "SAFETYMSG"


void configuration_init(model_t *pmodel) {
    uint16_t value       = 0;
    uint32_t value_32bit = 0;

    if (storage_load_uint16(&value, ADDRESS_KEY) == 0) {
        model_set_address(pmodel, value);
    }
    if (storage_load_uint32(&value_32bit, SERIAL_NUM_KEY) == 0) {
        model_set_serial_number(pmodel, value_32bit);
    }
    if (storage_load_uint16(&value, CLASS_KEY) == 0) {
        model_set_class(pmodel, value, NULL);
    }

    storage_load_blob(pmodel->safety_message, sizeof(pmodel->safety_message), SAFETY_MESSAGE_KEY);
}


void configuration_save_serial_number(void *args, uint32_t value) {
    storage_save_uint32(&value, SERIAL_NUM_KEY);
    model_set_serial_number(args, value);
}


void configuration_save_address(void *args, uint16_t value) {
    storage_save_uint16(&value, ADDRESS_KEY);
    model_set_address(args, value);
}


void configuration_save_safety_message(void *args, const char *string) {
    storage_save_blob((char *)string, strlen(string), SAFETY_MESSAGE_KEY);
    model_set_safety_message(args, string);
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