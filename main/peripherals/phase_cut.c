#include "driver/gpio.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "hardwareprofile.h"
#include "esp_log.h"
#include "phase_cut.h"


static void zcross_isr_handler(void *arg);
static void start_activate_triac_timer(size_t usecs);
static void start_deactivate_triac_timer(void);

static uint64_t period = 0;

static const char *TAG = "Phase cut";


#define TIMER_ZCROSS_GROUP    TIMER_GROUP_0
#define TIMER_ZCROSS_IDX      TIMER_0
#define TIMER_TRIAC_OFF_GROUP TIMER_GROUP_1
#define TIMER_TRIAC_OFF_IDX   TIMER_0
#define TIMER_RESOLUTION_HZ   1000000     // 1MHz resolution
#define TIMER_USECS(x)        (x * (TIMER_RESOLUTION_HZ / 1000000))
#define TIMER_TRIAC_OFF_ALARM TIMER_USECS(100)


static bool IRAM_ATTR timer_triac_off_callback(void *args) {
    (void)args;
    timer_pause(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX);
    gpio_set_level(IO_L1, 0);
    gpio_set_level(IO_L2, 0);
    gpio_set_level(IO_L3, 0);
    return 0;
}


static bool IRAM_ATTR timer_zerocross_callback(void *args) {
    (void)args;
    timer_pause(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX);
    gpio_set_level(IO_L1, 1);
    gpio_set_level(IO_L2, 1);
    gpio_set_level(IO_L3, 1);
    start_deactivate_triac_timer();
    return 0;
}


static void start_deactivate_triac_timer(void) {
    // For the timer counter to a initial value
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX, 0));
    // Set alarm value and enable alarm interrupt
    ESP_ERROR_CHECK(timer_set_alarm(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX, 1));
    // Start timer
    ESP_ERROR_CHECK(timer_start(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX));
}

static void start_activate_triac_timer(size_t usecs) {
    // For the timer counter to a initial value
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 0));
    // Set alarm value and enable alarm interrupt
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, TIMER_USECS(usecs)));
    ESP_ERROR_CHECK(timer_set_alarm(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 1));
    // Start timer
    ESP_ERROR_CHECK(timer_start(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX));
}


void phase_cut_set_period(uint64_t usecs) {
    period = usecs;
}


void phase_cut_init(void) {
    timer_config_t config = {
        .clk_src     = TIMER_SRC_CLK_APB,
        .divider     = APB_CLK_FREQ / TIMER_RESOLUTION_HZ,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en  = TIMER_PAUSE,
        .alarm_en    = TIMER_ALARM_EN,
        .auto_reload = false,
    };
    ESP_ERROR_CHECK(timer_init(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, &config));
    ESP_ERROR_CHECK(timer_init(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX, &config));

    ESP_ERROR_CHECK(timer_isr_callback_add(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, timer_zerocross_callback, NULL, 0));
    ESP_ERROR_CHECK(timer_enable_intr(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX));

    ESP_ERROR_CHECK(
        timer_isr_callback_add(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX, timer_triac_off_callback, NULL, 0));
    ESP_ERROR_CHECK(timer_enable_intr(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX));

    // For the timer counter to a initial value
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 0));
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX, 0));

    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_TRIAC_OFF_GROUP, TIMER_TRIAC_OFF_IDX, TIMER_TRIAC_OFF_ALARM));

    gpio_config_t io_conf_input = {
        // interrupt of falling edge
        .intr_type = GPIO_INTR_NEGEDGE,
        // bit mask of the pins
        .pin_bit_mask = (1ULL << IO_ZCROS),
        // set as input mode
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
    };
    gpio_config(&io_conf_input);

    gpio_config_t io_conf_output = {
        // interrupt of falling edge
        .intr_type = GPIO_INTR_DISABLE,
        // bit mask of the pins
        .pin_bit_mask = (1ULL << IO_L1) | (1ULL << IO_L2) | (1ULL << IO_L3),
        // set as input mode
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
    };
    gpio_config(&io_conf_output);

    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(IO_ZCROS, zcross_isr_handler, NULL);
}


static void IRAM_ATTR zcross_isr_handler(void *arg) {
    gpio_set_level(IO_L1, 0);
    gpio_set_level(IO_L2, 0);
    gpio_set_level(IO_L3, 0);
    start_activate_triac_timer(period);
}
