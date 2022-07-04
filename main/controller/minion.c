#include "minion.h"
#include "../components/liblightmodbus-esp/src/esp.config.h"
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
#include "peripherals/rs485.h"
#include "easyconnect.h"
#include "motor.h"
#include "model/model.h"


#define REG_COUNT 6

#define HOLDING_REGISTER_SPEED_STEP 4

#define COIL_MOTOR_STATE 0


static ModbusError           register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                                               ModbusRegisterCallbackResult *result);
static ModbusError           exception_callback(const ModbusSlave *minion, uint8_t function, ModbusExceptionCode code);
static LIGHTMODBUS_RET_ERROR initialization_function(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                     uint8_t requestLength);


static const ModbusSlaveFunctionHandler custom_functions[] = {
#if defined(LIGHTMODBUS_F01S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {1, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F02S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {2, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F03S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {3, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F04S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {4, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F05S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {5, modbusParseRequest0506},
#endif
#if defined(LIGHTMODBUS_F06S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {6, modbusParseRequest0506},
#endif
#if defined(LIGHTMODBUS_F15S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {15, modbusParseRequest1516},
#endif
#if defined(LIGHTMODBUS_F16S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {16, modbusParseRequest1516},
#endif
#if defined(LIGHTMODBUS_F22S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {22, modbusParseRequest22},
#endif

    {EASYCONNECT_FUNCTION_CODE_CONFIG_ADDRESS, easyconnect_set_address_function},
    {EASYCONNECT_FUNCTION_CODE_RANDOM_SERIAL_NUMBER, easyconnect_send_address_function},
    {EASYCONNECT_FUNCTION_CODE_NETWORK_INIZIALIZATION, initialization_function},
    {EASYCONNECT_FUNCTION_CODE_SET_CLASS_OUTPUT, easyconnect_set_class_output},

    // Guard - prevents 0 array size
    {0, NULL},
};

static const char *TAG = "Minion";
static ModbusSlave minion;


void minion_init(easyconnect_interface_t *context) {
    ESP_LOGI(TAG, "Minion address %i", context->get_address(context->arg));

    ModbusErrorInfo err;
    err =
        modbusSlaveInit(&minion,
                        register_callback,          // Callback for register operations
                        exception_callback,         // Callback for handling minion exceptions (optional)
                        modbusDefaultAllocator,     // Memory allocator for allocating responses
                        custom_functions,           // Set of supported functions
                        sizeof(custom_functions) / sizeof(custom_functions[0]) - 1     // Number of supported functions
        );

    // Check for errors
    assert(modbusIsOk(err) && "modbusSlaveInit() failed");
    modbusSlaveSetUserPointer(&minion, context);
}


void minion_manage(void) {
    uint8_t buffer[256] = {0};
    int     len         = rs485_read(buffer, sizeof(buffer));

    easyconnect_interface_t *context = modbusSlaveGetUserPointer(&minion);

    if (len > 0) {
        ModbusErrorInfo err;
        err = modbusParseRequestRTU(&minion, context->get_address(context->arg), buffer, len);

        if (modbusIsOk(err)) {
            size_t rlen = modbusSlaveGetResponseLength(&minion);
            if (rlen > 0) {
                rs485_write((uint8_t *)modbusSlaveGetResponse(&minion), rlen);
            } else {
                ESP_LOGD(TAG, "Empty response");
            }
        } else if (err.error != MODBUS_ERROR_ADDRESS) {
            ESP_LOGW(TAG, "Invalid request with source %i and error %i", err.source, err.error);
            ESP_LOG_BUFFER_HEX(TAG, buffer, len);
        }
    }
}


static ModbusError register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                                     ModbusRegisterCallbackResult *result) {

    easyconnect_interface_t *context = modbusSlaveGetUserPointer(status);
    result->exceptionCode            = MODBUS_EXCEP_NONE;

    ESP_LOGD(TAG, "%i %i %i %i\n", args->query, args->type, args->index, args->value);
    switch (args->query) {
        // R/W access check
        case MODBUS_REGQ_R_CHECK:
            if (args->type == MODBUS_INPUT_REGISTER) {
                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
            }
            result->exceptionCode = args->index < REG_COUNT ? MODBUS_EXCEP_NONE : MODBUS_EXCEP_ILLEGAL_ADDRESS;
            break;

        case MODBUS_REGQ_W_CHECK: {
            if (args->type == MODBUS_INPUT_REGISTER) {
                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
                break;
            }

            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    if (args->index >= REG_COUNT) {
                        result->exceptionCode = MODBUS_EXCEP_ILLEGAL_ADDRESS;
                        break;
                    }

                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                            if (args->value == 0 || args->value > 255) {
                                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_VALUE;
                            }
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_CLASS: {
                            uint8_t mode = (args->value & 0xFF00) >> 8;
                            if (mode != DEVICE_MODE_FAN) {
                                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_VALUE;
                            }
                            break;
                        }

                        case HOLDING_REGISTER_SPEED_STEP:
                            if (args->value >= NUM_SPEED_STEPS) {
                                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_VALUE;
                            }
                            break;


                        default:
                            break;
                    }
                    break;
                }

                case MODBUS_COIL:
                    if (args->index >= 1) {
                        result->exceptionCode = MODBUS_EXCEP_ILLEGAL_ADDRESS;
                    }
                    break;

                default:
                    result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
                    break;
            }
            break;
        }

        // Read register
        case MODBUS_REGQ_R:
            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                            result->value = context->get_address(context->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_CLASS:
                            result->value = context->get_class(context->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER:
                            result->value = context->get_serial_number(context->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_ALARM:
                            result->value = 0;
                            break;

                        case HOLDING_REGISTER_SPEED_STEP:
                            result->value = model_get_speed_step(context->arg);
                            break;
                    }
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
                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                            context->save_address(context->arg, args->value);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER:
                            context->save_serial_number(context->arg, args->value);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_CLASS: {
                            uint8_t mode = (args->value & 0xFF00) >> 8;
                            if (mode == DEVICE_MODE_FAN) {
                                context->save_class(context->arg, args->value);
                            }
                            break;
                        }

                        case HOLDING_REGISTER_SPEED_STEP:
                            ESP_LOGI(TAG, "Speed %i", args->value);
                            if (args->value < NUM_SPEED_STEPS) {
                                model_set_speed_step(context->arg, args->value);
                                if (motor_get_state()) {
                                    motor_set_speed(model_get_current_speed(context->arg));
                                }
                            }
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_ALARM:
                            break;
                    }
                    break;
                }

                case MODBUS_COIL:
                    switch (args->index) {
                        case COIL_MOTOR_STATE:
                            context->set_output(context->arg, args->value);
                            break;
                    }
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


static ModbusError exception_callback(const ModbusSlave *minion, uint8_t function, ModbusExceptionCode code) {
    ESP_LOGW(TAG, "Minion reports an exception %d (function %d)", code, function);
    // Always return MODBUS_OK
    return MODBUS_OK;
}


static LIGHTMODBUS_RET_ERROR initialization_function(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                     uint8_t requestLength) {
    easyconnect_interface_t *context = modbusSlaveGetUserPointer(minion);
    model_set_motor_control_override(context->arg, 1);
    phase_cut_stop();
    return MODBUS_NO_ERROR();
}
