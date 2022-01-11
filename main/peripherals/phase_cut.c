#include "driver/gpio.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "hal/gpio_types.h"
#include "hardwareprofile.h"
#include "esp_log.h"
#include "phase_cut.h"
#include <stdbool.h>
#include <sys/_stdint.h>

static void zcross_isr_handler(void *arg);
static void add_counter(uint32_t *counters, uint32_t counter);

static volatile uint32_t period = 0;
#define NUM_COUNTERS 3
static volatile uint32_t p1_counters[NUM_COUNTERS] = {0};
static volatile size_t p1_triac = 0;
static int full = 0;

static const char *TAG = "Phase cut";


#define TIMER_ZCROSS_GROUP    TIMER_GROUP_0
#define TIMER_ZCROSS_IDX      TIMER_0
#define TIMER_RESOLUTION_HZ   1000000     // 1MHz resolution
#define TIMER_USECS(x)        (x * (TIMER_RESOLUTION_HZ / 1000000))
#define TIMER_PERIOD TIMER_USECS(100)
#define PHASE_HALFPERIOD TIMER_USECS(10000)


#define MAX_PERIOD 9500UL
#define MIN_PERIOD 500UL


static bool IRAM_ATTR timer_phasecut_callback(void *args) {
    (void)args;
    //p1_counter-=100;
    //p2_counter-=100;
    //p3_counter=-100;
    
    int i;

    if (p1_triac) {
        gpio_set_level(IO_L1, 0);
        p1_triac=0;
    }

    for (i=0;i<NUM_COUNTERS;i++) {
        if (p1_counters[i]>0) {
            if (p1_counters[i] > 100) {
                p1_counters[i] -= 100;
            } else {
                p1_counters[i] = 0;
            }
            if (p1_counters[i]==0) {
                gpio_set_level(IO_L1, 1);
                p1_triac=1;
            }
        } 
    }

    gpio_set_level(IO_L2, 1);
    gpio_set_level(IO_L3, 1);

    return 0;
}

void phase_cut_set_period(uint32_t usecs) {
    period = usecs;
    ESP_LOGI(TAG,"%lu", period);
}

void phase_cut_timer_enable(int enable) {
    if (enable) {
        // For the timer counter to a initial value
        ESP_ERROR_CHECK(timer_set_counter_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, 0));
        // Start timer
        ESP_ERROR_CHECK(timer_start(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX));
    }
    else if (!enable) {
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
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_ZCROSS_GROUP, TIMER_ZCROSS_IDX, TIMER_PERIOD));
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




void phase_cut_set_percentage(unsigned int perc) {
    if (perc>=100) {
        perc=100;
        full=1;
        phase_cut_timer_enable(0);
    } else if (perc==0) {
        full=0;
        phase_cut_timer_enable(0);
    } else {
        full=0;
        perc=100-perc;
        unsigned int range = (MAX_PERIOD-MIN_PERIOD) / 100;
        period = MIN_PERIOD + perc*range; //MIN_PERIOD+((perc*range)/100);
    }
}


static void IRAM_ATTR zcross_isr_handler(void *arg) {
    if (full) {
        gpio_set_level(IO_L1, 1);
        gpio_set_level(IO_L2, 1);
        gpio_set_level(IO_L3, 1);
    } else {
        gpio_set_level(IO_L1, 0);
        gpio_set_level(IO_L2, 0);
        gpio_set_level(IO_L3, 0);
    }

    add_counter(p1_counters, period);
    //p2_counter=(PHASE_HALFPERIOD*2)/3 + period;
    //p3_counter=(PHASE_HALFPERIOD*4)/3 + period;
}

static void add_counter(uint32_t *counters, uint32_t counter) {
    int i;
    for (i=0;i<NUM_COUNTERS;i++) {
        if (counters[i]==0) {
            counters[i]=counter;
            break;
        }
    }
}

