#include "driver/gpio.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "hal/gpio_types.h"
#include "hardwareprofile.h"
#include "esp_log.h"
#include "phase_cut.h"
#include <stdbool.h>
#include <sys/_stdint.h>


#define TIMER_ZCROSS_GROUP      TIMER_GROUP_0
#define TIMER_ZCROSS_IDX        TIMER_0
#define TIMER_RESOLUTION_HZ     1000000     // 1MHz resolution
#define TIMER_USECS(x)          (x * (TIMER_RESOLUTION_HZ / 1000000))
#define INTERRUPT_PERIOD_USECS  100
#define PHASE_HALFPERIOD        TIMER_USECS(10000)
#define PERIOD_CORRECTION_USECS 50

#define MAX_PERIOD   9500UL
#define MIN_PERIOD   4000UL     // 1500UL
#define NUM_COUNTERS 3


static void zcross_isr_handler(void *arg);
static void add_counter(uint32_t *counters, uint32_t counter);

static const uint16_t sine_percentage_linearization[100] = {
    9999, 9361, 9095, 8890, 8716, 8563, 8423, 8294, 8173, 8058, 7950, 7846, 7746, 7650, 7556, 7466, 7378,
    7292, 7208, 7126, 7046, 6967, 6889, 6813, 6738, 6664, 6591, 6519, 6447, 6377, 6307, 6238, 6169, 6101,
    6034, 5967, 5900, 5834, 5768, 5703, 5638, 5573, 5508, 5444, 5380, 5315, 5252, 5188, 5124, 5060, 4996,
    4933, 4869, 4805, 4741, 4677, 4613, 4549, 4485, 4420, 4355, 4290, 4225, 4159, 4093, 4026, 3959, 3892,
    3824, 3755, 3686, 3616, 3546, 3474, 3402, 3329, 3255, 3180, 3104, 3026, 2947, 2867, 2785, 2701, 2615,
    2527, 2437, 2343, 2247, 2147, 2043, 1935, 1820, 1699, 1570, 1430, 1276, 1103, 898,  632,
};


static volatile uint32_t phase_halfperiod          = PHASE_HALFPERIOD;
volatile uint32_t        current_phase_halfperiod  = 0;
static volatile uint32_t period                    = 0;
static volatile uint32_t p1_counters[NUM_COUNTERS] = {0};
static volatile uint32_t p2_counters[NUM_COUNTERS] = {0};
static volatile uint32_t p3_counters[NUM_COUNTERS] = {0};
static int               full                      = 0;
static int               turn_off_counter[3]       = {0};

static const char *TAG = "Phase cut";


static bool IRAM_ATTR timer_phasecut_callback(void *args) {
    (void)args;

    current_phase_halfperiod += INTERRUPT_PERIOD_USECS;

    if (turn_off_counter[0] > 0) {
        turn_off_counter[0]--;
    }
    if (turn_off_counter[1] > 0) {
        turn_off_counter[1]--;
    }
    if (turn_off_counter[2] > 0) {
        turn_off_counter[2]--;
    }

    if (turn_off_counter[0] == 0 && !full) {
        gpio_set_level(IO_L1, 0);
    }
    if (turn_off_counter[1] == 0 && !full) {
        gpio_set_level(IO_L2, 0);
    }
    if (turn_off_counter[2] == 0 && !full) {
        gpio_set_level(IO_L3, 0);
    }

    for (size_t i = 0; i < NUM_COUNTERS; i++) {
        if (p1_counters[i] > 0) {
            if (p1_counters[i] > 100) {
                p1_counters[i] -= 100;
            } else {
                p1_counters[i] = 0;
            }
            if (p1_counters[i] == 0) {
                gpio_set_level(IO_L1, 1);
                turn_off_counter[0] = 2;
            }
        }

        if (p2_counters[i] > 0) {
            if (p2_counters[i] > 100) {
                p2_counters[i] -= 100;
            } else {
                p2_counters[i] = 0;
            }
            if (p2_counters[i] == 0) {
                gpio_set_level(IO_L2, 1);
                turn_off_counter[1] = 2;
            }
        }

        if (p3_counters[i] > 0) {
            if (p3_counters[i] > 100) {
                p3_counters[i] -= 100;
            } else {
                p3_counters[i] = 0;
            }
            if (p3_counters[i] == 0) {
                gpio_set_level(IO_L3, 1);
                turn_off_counter[2] = 2;
            }
        }
    }

    return 0;
}


void phase_cut_set_period(uint32_t usecs) {
    period = TIMER_USECS(usecs);
}


uint32_t phase_cut_get_period(void) {
    return period;
}


void phase_cut_timer_enable(int enable) {
    if (enable) {
        // For the timer counter to a initial value
        ESP_ERROR_CHECK(timer_set_counter_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 0));
        // Start timer
        ESP_ERROR_CHECK(timer_start(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX));
    } else if (!enable) {
        timer_pause(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX);
    }
}


void phase_cut_init(void) {
    timer_config_t config = {
        .clk_src     = TIMER_SRC_CLK_APB,
        .divider     = APB_CLK_FREQ / TIMER_RESOLUTION_HZ,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en  = TIMER_PAUSE,
        .alarm_en    = TIMER_ALARM_EN,
        .auto_reload = true,
    };
    ESP_ERROR_CHECK(timer_init(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, &config));
    ESP_ERROR_CHECK(timer_isr_callback_add(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, timer_phasecut_callback, NULL, 0));
    ESP_ERROR_CHECK(timer_enable_intr(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX));

    // For the timer counter to a initial value
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 0));
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, TIMER_USECS(INTERRUPT_PERIOD_USECS)));
    ESP_ERROR_CHECK(timer_set_alarm(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 1));

    ESP_ERROR_CHECK(timer_start(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX));


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


void phase_cut_stop(void) {
    full = 0;
    phase_cut_timer_enable(0);
}


void phase_cut_set_percentage(unsigned int perc) {
    if (perc >= 100) {
        perc = 99;
    }
    uint16_t converted_period = sine_percentage_linearization[perc];

    if (converted_period < MIN_PERIOD) {
        full = 1;
        phase_cut_timer_enable(0);
    } else if (converted_period > MAX_PERIOD) {
        full = 0;
        phase_cut_timer_enable(0);
    } else {
        full = 0;
        phase_cut_timer_enable(1);
        phase_cut_set_period(converted_period);
    }

#if 0
    if (perc >= 100) {
        perc = 100;
        full = 1;
        phase_cut_timer_enable(0);
    } else if (perc == 0) {
        full = 0;
        phase_cut_timer_enable(0);
    } else {
        phase_cut_timer_enable(1);
        full                     = 0;
        perc                     = 100 - perc;
        const unsigned int range = (MAX_PERIOD - MIN_PERIOD) / 100;
        phase_cut_set_period(MIN_PERIOD + perc * range);
    }
#endif
}


static void IRAM_ATTR zcross_isr_handler(void *arg) {
    if (full) {
        gpio_set_level(IO_L1, 1);
        gpio_set_level(IO_L2, 1);
        gpio_set_level(IO_L3, 1);
    } else {
        // gpio_set_level(IO_L1, 0);
        // gpio_set_level(IO_L2, 0);
        // gpio_set_level(IO_L3, 0);
    }

    if (current_phase_halfperiod > phase_halfperiod) {
        phase_halfperiod += PERIOD_CORRECTION_USECS;
    } else if (current_phase_halfperiod < phase_halfperiod) {
        phase_halfperiod -= PERIOD_CORRECTION_USECS;
    }
    current_phase_halfperiod = 0;
    phase_halfperiod         = PHASE_HALFPERIOD;

    add_counter((uint32_t *)p1_counters, period);
    // add_counter((uint32_t *)p2_counters, ((PHASE_HALFPERIOD * 2) / 3) + period);
    // add_counter((uint32_t *)p3_counters, ((PHASE_HALFPERIOD * 4) / 3) + period);
    add_counter((uint32_t *)p2_counters, ((phase_halfperiod * 2) / 3) + period);
    add_counter((uint32_t *)p3_counters, ((phase_halfperiod * 4) / 3) + period);
}

static void add_counter(uint32_t *counters, uint32_t counter) {
    int i;
    for (i = 0; i < NUM_COUNTERS; i++) {
        if (counters[i] == 0) {
            counters[i] = counter;
            break;
        }
    }
}
