#include <string.h>
#include <stdio.h>
#include "controller/minion.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include <esp_vfs_dev.h>
#include "driver/uart.h"
#include "controller/minion.h"
#include "model/model.h"
#include "controller/controller.h"
#include "peripherals/system.h"
#include "peripherals/rs485.h"
#include "peripherals/heartbeat.h"
#include "peripherals/phase_cut.h"
#include "peripherals/controllo_digitale.h"
#include "peripherals/storage.h"


static const char *TAG = "Main";

void app_main(void) {
    model_t model;

    system_random_init();
    rs485_init();
    heartbeat_init();
    phase_cut_init();
    controllo_digitale_init();
    storage_init();

    model_init(&model);
    controller_init(&model);

    ESP_LOGI(TAG, "Begin main loop");
    for (;;) {
        controller_manage(&model);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
