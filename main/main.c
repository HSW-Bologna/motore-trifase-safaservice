#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include <esp_vfs_dev.h>

#include "model/model.h"
#include "controller/controller.h"
#include "peripherals/heartbeat.h"
#include "peripherals/phase_cut.h"


static const char *TAG = "Main";

void app_main(void) {
    model_t model;

    heartbeat_init(1000);
    phase_cut_init();

    model_init(&model);
    // view_init(&model);
    controller_init(&model);

    // esp_vfs_usb_serial_jtag_use_driver();

    /*FILE *f = fopen("/dev/usbserjtag", "w+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Unable to open serial");
    } else {
    }
    const char *msg = "ciao seriale usb\n";
    fwrite(msg, 1, strlen(msg), f);*/
    uint64_t period = 0;


    ESP_LOGI(TAG, "Begin main loop");
    for (;;) {
        //ESP_LOGI(TAG, "setting period %llu", period);
        phase_cut_set_period(2000);
        //period = (period + 500) % 10000;
        // ESP_LOGI(TAG, "Hello world!");

        /*char readmsg[8] = {0};
        int  res        = fread(readmsg, 1, sizeof(readmsg), f);
        if (res > 0) {
            ESP_LOGI(TAG, "Letti %i: %s", res, readmsg);
        }*/
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
