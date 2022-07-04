#include "controllo_digitale.h"
#include "driver/gpio.h"
#include "driver/adc_common.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "hardwareprofile.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "gel/debounce/debounce.h"
#include <sys/_stdint.h>
#include "esp_log.h"
#include "esp_adc_cal.h"


#define READ_ON_PERIOD    10
#define READ_SPEED_PERIOD 100
#define MAX_ANALOG        3940
#define MIN_ANALOG        1240

#define NUM_READINGS 5

static bool adc_calibration_init(void);

static const char *TAG = "Digital control";

static SemaphoreHandle_t sem = NULL;

static uint16_t speeds[NUM_READINGS];
static size_t   avarage_index = 0;
static size_t   first_loop    = 1;

static debounce_filter_t filter;
static TimerHandle_t     timer_read_on    = NULL;
static TimerHandle_t     timer_read_speed = NULL;


static void controllo_digitale_read_signal_on(void *arg) {     // da fare periodicamente
    (void)arg;
    int signal = gpio_get_level(IO_VEL_ON);
    debounce_filter(&filter, signal, 5);
}


static void controllo_digitale_read_analog_speed(void *arg) {     // da fare periodicamente
    (void)arg;
    uint16_t speed        = adc1_get_raw(ANALOG_SPEED);
    speeds[avarage_index] = speed;
    if (avarage_index == NUM_READINGS - 1) {
        first_loop = 0;
    }
    avarage_index = (avarage_index + 1) % NUM_READINGS;
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

    adc_calibration_init();

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);

    debounce_filter_init(&filter);

    static StaticSemaphore_t semaphore_buffer;
    sem = xSemaphoreCreateMutexStatic(&semaphore_buffer);

    timer_read_on =
        xTimerCreate("read_on", pdMS_TO_TICKS(READ_ON_PERIOD), pdTRUE, NULL, controllo_digitale_read_signal_on);
    timer_read_speed = xTimerCreate("read speed", pdMS_TO_TICKS(READ_SPEED_PERIOD), pdTRUE, NULL,
                                    controllo_digitale_read_analog_speed);
    xTimerStart(timer_read_on, portMAX_DELAY);
    xTimerStart(timer_read_speed, portMAX_DELAY);
}


uint8_t controllo_digitale_get_signal_on(void) {
    xSemaphoreTake(sem, portMAX_DELAY);
    uint8_t res = !debounce_value(&filter);
    xSemaphoreGive(sem);
    return res;
}


uint16_t controllo_digitale_get_analog_speed(void) {
    size_t   i;
    uint16_t res;
    uint32_t speed_sum = 0;

    xSemaphoreTake(sem, portMAX_DELAY);
    size_t num_readings = first_loop ? avarage_index : NUM_READINGS;
    for (i = 0; i < num_readings; i++) {
        speed_sum += speeds[i];
    }
    if (num_readings == 0) {
        res = 0;
    } else {
        res = (uint16_t)(speed_sum / num_readings);
    }
    xSemaphoreGive(sem);
    return res;
}


int controllo_digitale_get_perc_speed(void) {
    int value = (int)controllo_digitale_get_analog_speed() - MIN_ANALOG;
    if (value < 0) {
        value = 0;
    }
    int perc = (100 * value) / (MAX_ANALOG - MIN_ANALOG);
    return perc;
}


static bool adc_calibration_init(void) {
    esp_err_t ret;
    bool      cali_enable = false;

    ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    } else if (ret == ESP_ERR_INVALID_VERSION) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else if (ret == ESP_OK) {
        cali_enable = true;
        esp_adc_cal_characteristics_t adc1_chars;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, &adc1_chars);
    } else {
        ESP_LOGE(TAG, "Invalid arg");
    }

    return cali_enable;
}