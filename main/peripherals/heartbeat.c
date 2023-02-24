#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hardwareprofile.h"
#include "heartbeat.h"


static const char *TAG = "Heartbeat";

void heartbeat_init(void) {
    gpio_set_direction(IO_LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(IO_LED_RED, GPIO_MODE_OUTPUT);
}


void heartbeat_update_green(uint8_t value) {
    gpio_set_level(IO_LED_GREEN, !value);
}


void heartbeat_update_red(uint8_t value) {
    gpio_set_level(IO_LED_RED, !value);
}