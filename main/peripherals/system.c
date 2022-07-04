#include <esp_random.h>
#include <bootloader_random.h>
#include <esp_system.h>


void system_random_init(void) {
    /* Initialize RNG address */
    bootloader_random_enable();
    srand(esp_random());
    bootloader_random_disable();
}