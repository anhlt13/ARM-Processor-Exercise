//
// Created by baotr on 9/30/2026.
//

#include <iostream>
#include <sstream>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hardware/gpio.h"
#include "PicoOsUart.h"
#include "ssd1306.h"
#include "sensor_task.h"
#include "network_task.h"


#include "hardware/timer.h"
extern "C" {
    uint32_t read_runtime_ctr(void) {
        return timer_hw->timerawl;
    }
}

// stack overflow check
extern "C" {
    void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName ) {
        if (pcTaskName != NULL) panic("Stack overflow: %s",pcTaskName);
        else panic("Stack overflow of unnamed task");
    }
}

#include "system_state.h"
#include "EEPROM.h"
#include "relay.h"
#include "ui_task.h"
#include "control_task.h"
#include "debug_task.h"
//#include "sensor_task.h"
static constexpr UBaseType_t PRIORITY_NETWORK = tskIDLE_PRIORITY + 5;
static constexpr UBaseType_t PRIORITY_CONTROL = tskIDLE_PRIORITY + 4;
static constexpr UBaseType_t PRIORITY_SENSOR = tskIDLE_PRIORITY + 3;
static constexpr UBaseType_t PRIORITY_UI = tskIDLE_PRIORITY + 2;
static constexpr UBaseType_t PRIORITY_DEBUG = tskIDLE_PRIORITY + 1;

static constexpr configSTACK_DEPTH_TYPE STACK_UI = 1024;
static constexpr configSTACK_DEPTH_TYPE STACK_CONTROL = 512;
static constexpr configSTACK_DEPTH_TYPE STACK_DEBUG = 512;

int main()
{
    stdio_init_all();

    eeprom_log_init();

    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

    relayInit();

    systemStateInit();
    debugInit();

    xTaskCreate(debugTaskFunction, "Debug", STACK_DEBUG, nullptr, PRIORITY_DEBUG, nullptr);
    xTaskCreate(uiTaskFunction, "UI", STACK_UI, nullptr, PRIORITY_UI, nullptr);
    xTaskCreate(controlTaskFunction, "Control", STACK_CONTROL, nullptr,PRIORITY_CONTROL, &controlTaskHandle);
    xTaskCreate(sensor_task, "Sensor", 1024, nullptr,PRIORITY_SENSOR, nullptr);
    xTaskCreate(network_task, "Network", 4096, nullptr,PRIORITY_NETWORK, nullptr);
    vTaskStartScheduler();

    for (;;) {}
}