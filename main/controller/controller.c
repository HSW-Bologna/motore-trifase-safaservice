#include "esp_log.h"
#include "controller.h"
#include "model/model.h"
#include "configuration.h"
#include "peripherals/rs485.h"
#include "esp32c3_commandline.h"
#include "esp_console.h"
#include "motor.h"
#include "minion.h"
#include "easyconnect_interface.h"
#include "device_commands.h"
#include "gel/timer/timecheck.h"
#include "utils/utils.h"
#include "app_config.h"
#include "safety.h"
#include "leds_activity.h"
#include "leds_communication.h"
#include "peripherals/heartbeat.h"


extern volatile uint32_t calculated_phase_halfperiod;
extern volatile uint32_t last_phase_halfperiod;


static void delay_ms(unsigned long ms);
static void console_task(void *args);


static const char *TAG = "Controller";

static easyconnect_interface_t context = {
    .save_serial_number = configuration_save_serial_number,
    .save_class         = configuration_save_class,
    .save_address       = configuration_save_address,
    .get_address        = model_get_address,
    .get_class          = model_get_class,
    .get_serial_number  = model_get_serial_number,
    .delay_ms           = delay_ms,
    .write_response     = rs485_write,
};


void controller_init(model_t *pmodel) {
    context.arg = pmodel;

    ESP_LOGD(TAG, "Initializing controller");
    motor_init(pmodel);
    configuration_init(pmodel);
    model_check_values(pmodel);
    minion_init(&context);

    static uint8_t      stack_buffer[APP_CONFIG_BASE_TASK_STACK_SIZE * 6];
    static StaticTask_t task_buffer;
    xTaskCreateStatic(console_task, "Console", sizeof(stack_buffer), &context, 1, stack_buffer, &task_buffer);
}


void controller_manage(model_t *pmodel) {
    static unsigned long ms100_ts = 0;

    minion_manage();

    if (is_expired(ms100_ts, get_millis(), 50UL)) {
        if (!safety_ok() || model_get_missing_heartbeat(pmodel)) {
            motor_turn_off(pmodel);
        } else {
            motor_refresh(pmodel);
        }

        ms100_ts = get_millis();
    }

    heartbeat_update_green(leds_communication_manage(get_millis(), !model_get_missing_heartbeat(pmodel)));
    heartbeat_update_red(leds_activity_manage(get_millis(), model_get_motor_active(pmodel), 1, safety_ok()));
}


static void delay_ms(unsigned long ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}


static void console_task(void *args) {
    const char              *prompt    = "EC-peripheral> ";
    easyconnect_interface_t *interface = args;

    esp32c3_commandline_init(interface);
    device_commands_register(interface->arg);
    esp_console_register_help_command();

    for (;;) {
        esp32c3_edit_cycle(prompt);
    }

    vTaskDelete(NULL);
}
