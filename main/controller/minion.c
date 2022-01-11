#include "minion.h"
#include "../components/liblightmodbus-esp/src/esp.config.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/projdefs.h"
#include "peripherals/hardwareprofile.h"
#include "lightmodbus/base.h"
#include "lightmodbus/lightmodbus.h"
#include "lightmodbus/slave.h"
#include "lightmodbus/slave_func.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "peripherals/phase_cut.h"
#include "peripherals/controllo_digitale.h"

typedef struct {
    uint16_t speed_perc;
    uint16_t speed_on;
} minion_context_t;

#define MODBUS_TIMEOUT          10
#define MB_PORTNUM              1
#define SLAVE_ADDR              1
#define SERIAL_NUMBER           2
#define MODEL_NUMBER            0
#define REG_COUNT               3

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged
// state on receive pin
#define ECHO_READ_TOUT (3)     // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

static const char *     TAG = "Minion";
static minion_context_t context;
ModbusSlave             slave;

ModbusError myRegisterCallback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                               ModbusRegisterCallbackResult *result);

ModbusError                  myExceptionCallback(const ModbusSlave *slave, uint8_t function, ModbusExceptionCode code);

void minion_init(void) {
    ModbusErrorInfo err;
    err = modbusSlaveInit(&slave,
                          myRegisterCallback,         // Callback for register operations
                          myExceptionCallback,        // Callback for handling slave exceptions (optional)
                          modbusDefaultAllocator,     // Memory allocator for allocating responses
                        modbusSlaveDefaultFunctions,     // Set of supported functions
                        modbusSlaveDefaultFunctionCount  // Number of supported functions
    );

    // Check for errors
    assert(modbusIsOk(err) && "modbusSlaveInit() failed");

    modbusSlaveSetUserPointer(&slave, &context);

    uart_config_t uart_config = {
        .baud_rate           = 115200,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(SLAVE_ADDR, &uart_config));

    uart_set_pin(MB_PORTNUM, MB_UART_TXD, MB_UART_RXD, MB_DERE, -1);
    ESP_ERROR_CHECK(uart_driver_install(MB_PORTNUM, 512, 512, 10, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(MB_PORTNUM, UART_MODE_RS485_HALF_DUPLEX));
    ESP_ERROR_CHECK(uart_set_rx_timeout(MB_PORTNUM, ECHO_READ_TOUT));
}

void minion_manage(void) {
    uint8_t buffer[256] = {0};
    int     len         = uart_read_bytes(MB_PORTNUM, buffer, 256, pdMS_TO_TICKS(MODBUS_TIMEOUT));
    if (len > 0) {
        ModbusErrorInfo err;
        err = modbusParseRequestRTU(&slave,SLAVE_ADDR, buffer, len);

        if (modbusIsOk(err)) {
            size_t rlen = modbusSlaveGetResponseLength(&slave);
            if (rlen > 0) {
                uart_write_bytes(MB_PORTNUM, modbusSlaveGetResponse(&slave), rlen);
            } else {
                ESP_LOGI(TAG, "Empty response");
            }
        } else if (err.error != MODBUS_ERROR_ADDRESS) {
            ESP_LOGW(TAG, "Invalid request with source %i and error %i", err.source, err.error);
            ESP_LOG_BUFFER_HEX(TAG, buffer, len);
        }
    }
}


ModbusError myRegisterCallback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                               ModbusRegisterCallbackResult *result) {

    minion_context_t *ctx = modbusSlaveGetUserPointer(status);

    printf("%i %i %i %i\n", args->query, args->type, args->index, args->value);
    switch (args->query) {
        // R/W access check
        case MODBUS_REGQ_R_CHECK:
        case MODBUS_REGQ_W_CHECK:
            // If result->exceptionCode of a read/write access query is not
            // MODBUS_EXCEP_NONE, an exception is reported by the slave. If
            // result->exceptionCode is not set, the behavior is undefined.
            if (args->type == MODBUS_INPUT_REGISTER) {
                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
            }
            result->exceptionCode = args->index < REG_COUNT ? MODBUS_EXCEP_NONE : MODBUS_EXCEP_ILLEGAL_ADDRESS;
            break;

        // Read register
        case MODBUS_REGQ_R:
            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    uint16_t registers[] = {ctx->speed_perc, ctx->speed_on};
                    result->value        = registers[args->index];
                    break;
                }
                case MODBUS_INPUT_REGISTER:
                    result->value = controllo_digitale_get_analog_speed();
                    break;
                case MODBUS_COIL:
                    break;
                case MODBUS_DISCRETE_INPUT:
                    break;
            }
            break;

        // Write register
        case MODBUS_REGQ_W:
            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    uint16_t *registers[]   = {&ctx->speed_perc, &ctx->speed_on};
                    *registers[args->index] = args->value;
                    break;
                }
                case MODBUS_COIL:
                    break;
                case MODBUS_INPUT_REGISTER:
                    break;
                default:
                    break;
            }
            break;
    }

    // Always return MODBUS_OK
    return MODBUS_OK;
}

ModbusError myExceptionCallback(const ModbusSlave *slave, uint8_t function, ModbusExceptionCode code) {
    printf("Slave reports an exception %d (function %d)\n", code, function);

    // Always return MODBUS_OK
    return MODBUS_OK;
}
