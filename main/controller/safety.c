#include "peripherals/digin.h"


uint8_t safety_ok(void) {
    return digin_get(DIGIN_SAFETY) != 0;
}