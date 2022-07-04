#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hardwareprofile.h"
#include "heartbeat.h"


#define NORMAL_PERIOD 2000UL
#define FASTER_PERIOD 500UL


static const char   *TAG   = "Heartbeat";
static TimerHandle_t timer = NULL;

/**
 * Timer di attivita'. Accende e spegne il led di attivita'
 */
static void heartbeat_timer(TimerHandle_t timer) {
    (void)TAG;
    static int blink = 0;

    gpio_set_level(IO_LED_RUN, blink);
    blink = !blink;
    xTimerChangePeriod(timer,
                       pdMS_TO_TICKS(blink ? 50 : (pvTimerGetTimerID(timer) > 0 ? FASTER_PERIOD : NORMAL_PERIOD)),
                       portMAX_DELAY);
}


void heartbeat_init(void) {
    gpio_set_direction(IO_LED_RUN, GPIO_MODE_OUTPUT);
    timer = xTimerCreate("idle", pdMS_TO_TICKS(1000), pdTRUE, (void *)(uintptr_t)1, heartbeat_timer);
    xTimerStart(timer, portMAX_DELAY);
}


void heartbeat_faster(uint8_t faster) {
    vTimerSetTimerID(timer, (void *)(uintptr_t)faster);
}