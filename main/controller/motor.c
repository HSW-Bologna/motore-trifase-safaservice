#include <driver/gpio.h>
#include <driver/ledc.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "peripherals/heartbeat.h"
#include "peripherals/hardwareprofile.h"
#include "motor.h"
#include "safety.h"
#include "utils/utils.h"
#include "gel/timer/timecheck.h"
#include "model/model.h"


#define PWM_MODE    LEDC_LOW_SPEED_MODE
#define PWM_CHANNEL LEDC_CHANNEL_0
#define PWM_TIMER   LEDC_TIMER_1


static void set_duty_percentage(uint8_t percentage);


static const char *TAG = "Motor";


void motor_init(model_t *pmodel) {
    gpio_config_t config = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(HAP_OUTPUT),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,     // resolution of PWM duty
        .freq_hz         = 1000,                  // frequency of PWM signal
        .speed_mode      = PWM_MODE,              // timer mode
        .timer_num       = PWM_TIMER,             // timer index
        .clk_cfg         = LEDC_AUTO_CLK,         // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .channel             = PWM_CHANNEL,
        .duty                = 0,
        .gpio_num            = HAP_PWM,
        .speed_mode          = PWM_MODE,
        .hpoint              = 0,
        .timer_sel           = PWM_TIMER,
        .flags.output_invert = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    motor_turn_off(pmodel);
    ESP_LOGI(TAG, "Initialized");
}


void motor_set_speed(model_t *pmodel, uint8_t percentage) {
    if (percentage > 100) {
        percentage = 100;
    }

    model_set_speed_percentage(pmodel, percentage);
    set_duty_percentage(percentage);
}


void motor_turn_off(model_t *pmodel) {
    model_set_motor_active(pmodel, 0);
    gpio_set_level(HAP_OUTPUT, 0);
    set_duty_percentage(0);
}


void motor_turn_on(model_t *pmodel) {
    model_set_motor_active(pmodel, 1);
    if (safety_ok()) {
        gpio_set_level(HAP_OUTPUT, 1);
        set_duty_percentage(model_get_speed_percentage(pmodel));
    }
}


void motor_refresh(model_t *pmodel) {
    if (model_get_motor_active(pmodel)) {
        set_duty_percentage(model_get_speed_percentage(pmodel));
        gpio_set_level(HAP_OUTPUT, 1);
    } else {
        set_duty_percentage(0);
        gpio_set_level(HAP_OUTPUT, 0);
    }
}


static void set_duty_percentage(uint8_t percentage) {
    if (percentage > 100) {
        percentage = 100;
    }

    uint32_t duty = (percentage * 1024) / 100;
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}