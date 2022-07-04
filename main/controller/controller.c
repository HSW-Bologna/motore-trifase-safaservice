#include "esp_log.h"
#include "controller.h"
#include "model/model.h"
#include "configuration.h"
#include "peripherals/rs485.h"
#include "peripherals/controllo_digitale.h"
#include "peripherals/phase_cut.h"
#include "motor.h"
#include "minion.h"
#include "easyconnect_interface.h"
#include "gel/timer/timecheck.h"
#include "utils/utils.h"



extern volatile uint32_t calculated_phase_halfperiod;
extern volatile uint32_t last_phase_halfperiod;


static uint16_t get_class(void *args);
static void     delay_ms(unsigned long ms);
static void     set_output(void *args, uint8_t value);


static const char *TAG = "Controller";

static easyconnect_interface_t context = {
    .save_serial_number = configuration_save_serial_number,
    .save_class         = configuration_save_group,
    .save_address       = configuration_save_address,
    .get_address        = model_get_address,
    .get_class          = get_class,
    .get_serial_number  = model_get_serial_number,
    .set_output         = set_output,
    .delay_ms           = delay_ms,
    .write_response     = rs485_write,
};


void controller_init(model_t *pmodel) {
    context.arg = pmodel;

    ESP_LOGD(TAG, "Initializing controller");
    configuration_init(pmodel);
    model_check_values(pmodel);
    motor_init();
    minion_init(&context);
}


void controller_manage(model_t *pmodel) {
    static unsigned long ms100_ts = 0;

    minion_manage();

    if (is_expired(ms100_ts, get_millis(), 100UL)) {
        if (!model_get_motor_control_override(pmodel)) {
            if (controllo_digitale_get_signal_on()) {
                unsigned int speed = controllo_digitale_get_perc_speed();
                motor_set_speed(speed);
                // ESP_LOGI(TAG, "VELOCITA' %i%% (%i) [%6i  %6i]", speed, phase_cut_get_period(),
                // calculated_phase_halfperiod, last_phase_halfperiod);
            } else {
                motor_turn_off();
            }
        }

        ms100_ts = get_millis();
    }
}


static uint16_t get_class(void *args) {
    return (DEVICE_MODE_FAN << 8) | model_get_group(args);
}


static void delay_ms(unsigned long ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}


static void set_output(void *args, uint8_t value) {
    ESP_LOGI(TAG, "Output %i", value);
    motor_control(value, model_get_current_speed(args));
    model_set_motor_control_override(args, 1);
}