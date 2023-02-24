#include "esp_err.h"
#include "esp_console.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "device_commands.h"
#include "peripherals/digin.h"
#include "model/model.h"
#include "configuration.h"
#include "easyconnect_interface.h"


static int device_commands_read_inputs(int argc, char **argv);
static int device_commands_read_safety_message(int argc, char **argv);
static int device_commands_set_safety_message(int argc, char **argv);


static model_t *model_ref = NULL;


void device_commands_register(model_t *pmodel) {
    model_ref = pmodel;

    const esp_console_cmd_t signal_cmd = {
        .command = "ReadSignals",
        .help    = "Read signals levels",
        .hint    = NULL,
        .func    = &device_commands_read_inputs,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&signal_cmd));

    const esp_console_cmd_t read_safety_message = {
        .command = "ReadSafetyMessage",
        .help    = "Print the configured safety warning",
        .hint    = NULL,
        .func    = &device_commands_read_safety_message,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_safety_message));

    const esp_console_cmd_t set_safety_message = {
        .command = "SetSafetyMessage",
        .help    = "Set a new safety warning",
        .hint    = NULL,
        .func    = &device_commands_set_safety_message,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_safety_message));
}

static int device_commands_read_inputs(int argc, char **argv) {
    struct arg_end *end;
    /* the global arg_xxx structs are initialised within the argtable */
    void *argtable[] = {
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        uint8_t value = (uint8_t)digin_get_inputs();
        printf("Safety=%i\n", (value & 0x01) > 0);
    } else {
        arg_print_errors(stdout, end, "Read device inputs");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}

static int device_commands_read_safety_message(int argc, char **argv) {
    struct arg_end *end;
    void           *argtable[] = {
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        char safety_message[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
        model_get_safety_message(model_ref, safety_message);
        printf("%s\n", safety_message);
    } else {
        arg_print_errors(stdout, end, "Read safety message");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_set_safety_message(int argc, char **argv) {
    struct arg_str *sm;
    struct arg_end *end;
    void           *argtable[] = {
        sm  = arg_str1(NULL, NULL, "<safety message>", "safety message"),
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        configuration_save_safety_message(model_ref, sm->sval[0]);
    } else {
        arg_print_errors(stdout, end, "Set safety message");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}

