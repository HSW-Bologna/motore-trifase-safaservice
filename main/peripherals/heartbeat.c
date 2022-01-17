#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hardwareprofile.h"
#include "heartbeat.h"

static const char   *TAG   = "Heartbeat";
static TimerHandle_t timer = NULL;

/**
 * Timer di attivita'. Accende e spegne il led di attivita'
 */
static void heartbeat_timer(void *arg) {
    (void)arg;
    (void)TAG;
    static int    blink   = 0;
    static size_t counter = 0;

    gpio_set_level(IO_LED_RUN, blink);
    blink = !blink;

    if (counter++ >= 10) {
        // ESP_LOGI(TAG, "Low water mark: %i, free memory %i", xPortGetMinimumEverFreeHeapSize(),
        // xPortGetFreeHeapSize());
        counter = 0;
    }
}


void heartbeat_init(size_t period_ms) {
    gpio_set_direction(IO_LED_RUN, GPIO_MODE_OUTPUT);
    timer = xTimerCreate("idle", pdMS_TO_TICKS(period_ms), pdTRUE, NULL, heartbeat_timer);
    xTimerStart(timer, portMAX_DELAY);
}


void heartbeat_period(size_t period_ms) {
    xTimerChangePeriod(timer, pdMS_TO_TICKS(period_ms), portMAX_DELAY);
}