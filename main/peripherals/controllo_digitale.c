#include "controllo_digitale.h"
#include "driver/gpio.h"
#include "driver/adc_common.h"
#include "freertos/projdefs.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "hardwareprofile.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "../components/generic_embedded_libs/gel/debounce/debounce.h"
#include <sys/_stdint.h>

#define READ_ON_PERIOD 10
#define READ_SPEED_PERIOD 50
#define MAX_ANALOG 3940
#define MIN_ANALOG 1240

#define NUM_READINGS 5
static uint16_t speeds[NUM_READINGS];
static size_t avarage_index=0;
static size_t first_loop=1;

static debounce_filter_t filter;
static TimerHandle_t timer_read_on = NULL;
static TimerHandle_t timer_read_speed = NULL;

static void controllo_digitale_read_signal_on(void *arg) { //da fare periodicamente
    (void)arg;
    int signal = gpio_get_level(IO_VEL_ON);
    debounce_filter(&filter, signal, 5);
}

static void controllo_digitale_read_analog_speed(void *arg) { //da fare periodicamente
    (void)arg;
    uint16_t speed = adc1_get_raw(ANALOG_SPEED);
    speeds[avarage_index]=speed;
    if (avarage_index==NUM_READINGS-1) {
        first_loop=0;
    }
    avarage_index=(avarage_index+1)%NUM_READINGS;  
}

void controllo_digitale_init(void) {
    gpio_config_t io_conf_input = {
        // interrupt of falling edge
        .intr_type = GPIO_INTR_NEGEDGE,
        // bit mask of the pins
        .pin_bit_mask = (1ULL << IO_VEL_ON),
        // set as input mode
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
    };
    gpio_config(&io_conf_input);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);

    debounce_filter_init(&filter);
    timer_read_on = xTimerCreate("read_on", pdMS_TO_TICKS(READ_ON_PERIOD), pdTRUE, NULL, controllo_digitale_read_signal_on);
    timer_read_speed = xTimerCreate("read speed", pdMS_TO_TICKS(READ_SPEED_PERIOD), pdTRUE, NULL, controllo_digitale_read_analog_speed);
    xTimerStart(timer_read_on, portMAX_DELAY);
    xTimerStart(timer_read_speed, portMAX_DELAY);

}


int controllo_digitale_get_signal_on(void) {
    return !debounce_value(&filter);
}

uint16_t controllo_digitale_get_analog_speed(void) {
    size_t i;
    uint16_t res;
    uint32_t speed_sum=0;
    size_t num_readings = first_loop ? avarage_index : NUM_READINGS;
    for (i=0;i<num_readings;i++) {
        speed_sum+=speeds[i];
    }
    if (num_readings==0) {
        res=0;
    }
    else {
        res=(uint16_t) (speed_sum/num_readings);
    }
    return res;
}

int controllo_digitale_get_perc_speed(void) {
    int value = (int)controllo_digitale_get_analog_speed() - MIN_ANALOG;
    if (value < 0) {
        value = 0;
    }
    int perc = (100*value)/ (MAX_ANALOG-MIN_ANALOG);
    return perc;
}