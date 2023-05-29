#include <sys/time.h>
#include <string.h>
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
#include "utils/utils.h"
#include "gel/timer/timecheck.h"
#include "gel/serializer/serializer.h"
#include "safety.h"
#include "peripherals/rs485.h"
#include "easyconnect.h"
#include "motor.h"
#include "model/model.h"
#include "app_config.h"


#define HOLDING_REGISTER_SPEED            EASYCONNECT_HOLDING_REGISTER_CUSTOM_START
#define HOLDING_REGISTER_SAFETY_MESSAGE   EASYCONNECT_HOLDING_REGISTER_MESSAGE_1
#define HOLDING_REGISTER_FEEDBACK_MESSAGE (HOLDING_REGISTER_SAFETY_MESSAGE + EASYCONNECT_MESSAGE_NUM_REGISTERS)

#define COIL_MOTOR_STATE 0


static ModbusError           register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                                               ModbusRegisterCallbackResult *result);
static ModbusError           exception_callback(const ModbusSlave *minion, uint8_t function, ModbusExceptionCode code);
static LIGHTMODBUS_RET_ERROR initialization_function(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                     uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR set_class_output(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                              uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR heartbeat_received(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR set_datetime(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
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
    {EASYCONNECT_FUNCTION_CODE_NETWORK_INITIALIZATION, initialization_function},
    {EASYCONNECT_FUNCTION_CODE_SET_CLASS_OUTPUT, set_class_output},
    {EASYCONNECT_FUNCTION_CODE_SET_TIME, set_datetime},
    {EASYCONNECT_FUNCTION_CODE_HEARTBEAT, heartbeat_received},

    // Guard - prevents 0 array size
    {0, NULL},
};

static const char   *TAG = "Minion";
static ModbusSlave   minion;
static unsigned long timestamp = 0;


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

    timestamp = get_millis();
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
        } else if (err.error != MODBUS_ERROR_ADDRESS && err.error != MODBUS_ERROR_CRC) {
            ESP_LOGW(TAG, "Invalid request with source %i and error %i", err.source, err.error);
            ESP_LOG_BUFFER_HEX(TAG, buffer, len);
        }
    }

    if (is_expired(timestamp, get_millis(), EASYCONNECT_HEARTBEAT_TIMEOUT)) {
        if (model_get_missing_heartbeat(context->arg) == 0) {
            model_set_missing_heartbeat(context->arg, 1);
        }
    }
}


static ModbusError register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                                     ModbusRegisterCallbackResult *result) {

    easyconnect_interface_t *context = modbusSlaveGetUserPointer(status);
    result->exceptionCode            = MODBUS_EXCEP_NONE;

    ESP_LOGD(TAG, "%i %i %i %i", args->query, args->type, args->index, args->value);
    switch (args->query) {
        // R/W access check
        case MODBUS_REGQ_R_CHECK:
            if (args->type == MODBUS_INPUT_REGISTER) {
                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
            }

            switch (args->index) {
                case EASYCONNECT_HOLDING_REGISTER_ADDRESS ... EASYCONNECT_HOLDING_REGISTER_LOGS_COUNTER:
                case EASYCONNECT_HOLDING_REGISTER_LOGS ... EASYCONNECT_HOLDING_REGISTER_MESSAGE_1 - 1:
                case EASYCONNECT_HOLDING_REGISTER_MESSAGE_1 ... EASYCONNECT_HOLDING_REGISTER_MESSAGE_2 - 1:
                case HOLDING_REGISTER_SPEED:
                    result->exceptionCode = MODBUS_EXCEP_NONE;
                    break;

                default:
                    result->exceptionCode = MODBUS_EXCEP_ILLEGAL_ADDRESS;
                    break;
            }
            break;

        case MODBUS_REGQ_W_CHECK: {
            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                            if (args->value == 0 || args->value > 255) {
                                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_VALUE;
                            }
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_CLASS: {
                            break;
                        }

                        case HOLDING_REGISTER_SPEED:
                            if (args->value > 100) {
                                result->exceptionCode = MODBUS_EXCEP_ILLEGAL_VALUE;
                            }
                            break;

                        case HOLDING_REGISTER_SAFETY_MESSAGE ... HOLDING_REGISTER_FEEDBACK_MESSAGE - 1: {
                            char msg[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
                            model_get_safety_message(context->arg, msg);
                            size_t i      = args->index - HOLDING_REGISTER_SAFETY_MESSAGE;
                            result->value = msg[i * 2] << 8 | msg[i * 2 + 1];
                            break;
                        }

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

                        case EASYCONNECT_HOLDING_REGISTER_FIRMWARE_VERSION:
                            result->value = EASYCONNECT_FIRMWARE_VERSION(APP_CONFIG_FIRMWARE_VERSION_MAJOR,
                                                                         APP_CONFIG_FIRMWARE_VERSION_MINOR,
                                                                         APP_CONFIG_FIRMWARE_VERSION_PATCH);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_CLASS:
                            result->value = context->get_class(context->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_1:
                            result->value = (context->get_serial_number(context->arg) >> 16) & 0xFFFF;
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_2:
                            result->value = context->get_serial_number(context->arg) & 0xFFFF;
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_ALARMS:
                            result->value = (safety_ok() == 0);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_STATE:
                            result->value =
                                model_get_motor_active(context->arg) | (model_get_speed_percentage(context->arg) << 8);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_LOGS_COUNTER:
                            result->value = 0;
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_LOGS ... EASYCONNECT_HOLDING_REGISTER_MESSAGE_1 - 1: {
                            result->value = 0;
                            break;
                        }

                        case EASYCONNECT_HOLDING_REGISTER_MESSAGE_1 ... EASYCONNECT_HOLDING_REGISTER_MESSAGE_2 - 1: {
                            char msg[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
                            model_get_safety_message(context->arg, msg);
                            size_t i      = args->index - EASYCONNECT_HOLDING_REGISTER_MESSAGE_1;
                            result->value = msg[i * 2] << 8 | msg[i * 2 + 1];
                            break;
                        }

                        case HOLDING_REGISTER_SPEED:
                            result->value = model_get_speed_percentage(context->arg);
                            break;
                    }
                    break;
                }
                case MODBUS_INPUT_REGISTER:
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

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_1: {
                            uint32_t current_serial_number = context->get_serial_number(context->arg);
                            context->save_serial_number(context->arg,
                                                        (args->value << 16) | (current_serial_number & 0xFFFF));
                            break;
                        }

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_2: {
                            uint32_t current_serial_number = context->get_serial_number(context->arg);
                            context->save_serial_number(context->arg,
                                                        args->value | (current_serial_number & 0xFFFF0000));
                            break;
                        }

                        case EASYCONNECT_HOLDING_REGISTER_CLASS: {
                            context->save_class(context->arg, args->value);
                            break;
                        }

                        case HOLDING_REGISTER_SPEED: {
                            uint16_t percentage = args->value;
                            if (percentage > 100) {
                                percentage = 100;
                            }
                            ESP_LOGI(TAG, "Speed %i", percentage);
                            motor_set_speed(context->arg, percentage);
                            break;
                        }
                    }
                    break;
                }

                case MODBUS_COIL:
                    switch (args->index) {
                        case COIL_MOTOR_STATE:
                            if (args->value) {
                                motor_turn_on(context->arg);
                            } else {
                                motor_turn_off(context->arg);
                            }
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
    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);
    motor_turn_off(ctx->arg);
    return MODBUS_NO_ERROR();
}


static LIGHTMODBUS_RET_ERROR set_class_output(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                              uint8_t requestLength) {
    // Check request length
    if (requestLength < 3) {
        return modbusBuildException(minion, function, MODBUS_EXCEP_ILLEGAL_VALUE);
    }

    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);
    uint16_t class               = requestPDU[1] << 8 | requestPDU[2];
    if (class == ctx->get_class(ctx->arg)) {
        ESP_LOGI(TAG, "Output %i, percentage %i", requestPDU[3], model_get_speed_percentage(ctx->arg));
        if (requestPDU[3]) {
            motor_turn_on(ctx->arg);
        } else {
            motor_turn_off(ctx->arg);
        }
    }

    return MODBUS_NO_ERROR();
}


static LIGHTMODBUS_RET_ERROR heartbeat_received(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                uint8_t requestLength) {
    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);

    timestamp = get_millis();
    model_set_missing_heartbeat(ctx->arg, 0);
    return MODBUS_NO_ERROR();
}


static LIGHTMODBUS_RET_ERROR set_datetime(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                          uint8_t requestLength) {
    // Check request length
    if (requestLength < 8) {
        return modbusBuildException(minion, function, MODBUS_EXCEP_ILLEGAL_VALUE);
    }

    uint64_t timestamp = 0;
    deserialize_uint64_be(&timestamp, (uint8_t *)requestPDU);

    struct timeval timeval = {0};
    timeval.tv_sec         = timestamp;

    settimeofday(&timeval, NULL);

    return MODBUS_NO_ERROR();
}