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
#include "peripherals/heartbeat.h"
#include "peripherals/phase_cut.h"
#include "peripherals/controllo_digitale.h"


static const char *TAG = "Main";

void app_main(void) {
    model_t model;
    heartbeat_init(1000);
    phase_cut_init();
    controllo_digitale_init();

    model_init(&model);
    // view_init(&model);
    controller_init(&model);
    minion_init();

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
        minion_manage();
        /*ESP_LOGI(TAG, "setting period %llu", period);
        phase_cut_set_period(period);
        period = (period + 500) % 10000;
        vTaskDelay(1000);
        continue;*/
        
        // ESP_LOGI(TAG, "Hello world!");

        /*char readmsg[8] = {0};
        int  res        = fread(readmsg, 1, sizeof(readmsg), f);
        if (res > 0) {
            ESP_LOGI(TAG, "Letti %i: %s", res, readmsg);
        }*/
        //vTaskDelay(pdMS_TO_TICKS(100));
        if (controllo_digitale_get_signal_on()) {
            unsigned int speed = controllo_digitale_get_perc_speed();
            ESP_LOGI(TAG, "VELOCITA' %i%%", speed);
            phase_cut_set_percentage(speed);
        }
        else {
            phase_cut_timer_enable(0);
        }
    }
}
