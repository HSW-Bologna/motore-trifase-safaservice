#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "peripherals/phase_cut.h"
#include "peripherals/heartbeat.h"
#include "motor.h"
#include "utils/utils.h"
#include "gel/timer/timecheck.h"


#define PERCENTAGE_ADJUSTMENT_STEP   5
#define PERCENTAGE_ADJUSTMENT_PERIOD 100
#define MINIMUM_PERCENTAGE           40


static void motor_timer(TimerHandle_t timer);


static const char       *TAG                  = "Motor";
static TimerHandle_t     timer                = NULL;
static SemaphoreHandle_t sem                  = NULL;
static uint8_t           actual_percentage    = MINIMUM_PERCENTAGE;
static uint8_t           objective_percentage = MINIMUM_PERCENTAGE;
static uint8_t           motor_on             = 0;
static uint8_t           bootstrap            = 0;
static unsigned long     bootstrap_ts         = 0;


void motor_init(void) {
    static StaticSemaphore_t semaphore_buffer;
    sem = xSemaphoreCreateMutexStatic(&semaphore_buffer);

    static StaticTimer_t timer_buffer;
    timer =
        xTimerCreateStatic(TAG, pdMS_TO_TICKS(PERCENTAGE_ADJUSTMENT_PERIOD), pdTRUE, NULL, motor_timer, &timer_buffer);
    xTimerStart(timer, portMAX_DELAY);

    motor_turn_off();
}


void motor_set_speed(uint8_t percentage) {
    if (percentage < MINIMUM_PERCENTAGE) {
        percentage = MINIMUM_PERCENTAGE;
    } else if (percentage > 100) {
        percentage = 100;
    }

    xSemaphoreTake(sem, portMAX_DELAY);
    // If the setup is always the same do nothing
    if (!motor_on || objective_percentage != percentage) {
        objective_percentage = percentage;
        if (!motor_on) {
            actual_percentage = 100;
            bootstrap         = 1;
            motor_on          = 1;
            bootstrap_ts      = get_millis();
            heartbeat_faster(1);
            phase_cut_set_percentage(actual_percentage);
            ESP_LOGI(TAG, "100%%");
        }
    }
    xSemaphoreGive(sem);
}


void motor_turn_off(void) {
    xSemaphoreTake(sem, portMAX_DELAY);
    motor_on = 0;
    xSemaphoreGive(sem);
    heartbeat_faster(0);
    phase_cut_set_percentage(0);
}


void motor_control(uint8_t enable, uint8_t percentage) {
    if (enable) {
        ESP_LOGI(TAG, "Setting speed to %i%%", percentage);
        motor_set_speed(percentage);
    } else {
        motor_turn_off();
    }
}


uint8_t motor_get_state(void) {
    xSemaphoreTake(sem, portMAX_DELAY);
    uint8_t res = motor_on;
    xSemaphoreGive(sem);
    return res;
}


static void motor_timer(TimerHandle_t timer) {
    uint8_t change     = 0;
    uint8_t percentage = 0;

    xSemaphoreTake(sem, portMAX_DELAY);
    if (bootstrap) {
        if (is_expired(bootstrap_ts, get_millis(), 1000UL)) {
            bootstrap = 0;
        }
    } else if (motor_on) {
        if (objective_percentage > actual_percentage) {
            if (objective_percentage > actual_percentage + PERCENTAGE_ADJUSTMENT_STEP) {
                actual_percentage += PERCENTAGE_ADJUSTMENT_STEP;
                change = 1;
            } else {
                actual_percentage = objective_percentage;
                change            = 1;
            }
        } else if (objective_percentage < actual_percentage) {
            if ((int8_t)objective_percentage < (int8_t)actual_percentage - (int8_t)PERCENTAGE_ADJUSTMENT_STEP) {
                actual_percentage -= PERCENTAGE_ADJUSTMENT_STEP;
                change = 1;
            } else {
                actual_percentage = objective_percentage;
                change            = 1;
            }
        }

        percentage = actual_percentage;
    }
    xSemaphoreGive(sem);

    if (change) {
        ESP_LOGI(TAG, "speed %i%%", percentage);
        phase_cut_set_percentage(percentage);
    }
}