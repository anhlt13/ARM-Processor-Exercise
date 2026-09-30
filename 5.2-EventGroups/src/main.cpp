//
// Created by Tran Lan Anh on 19.9.2026.
//
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "queue.h"
#include "event_groups.h"
#include "cstdlib"

// this is needed for runtime statistics
#include <iostream>
#include <ostream>

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

#define BUTTON1 9
#define BUTTON2 8
#define BUTTON3 7
#define TASK1_BIT 1 << 0
#define TASK2_BIT 1 << 1
#define TASK3_BIT 1 << 2
#define ALL_TASK_BITS (TASK1_BIT | TASK2_BIT | TASK3_BIT)
#define WATCHDOG_TIME pdMS_TO_TICKS(30000)

QueueHandle_t syslog_q;
EventGroupHandle_t eventGroup;

class ButtonState
{
private:
    bool last_state;
    bool button_state;
    TickType_t last_debounce_time;

public:
    ButtonState(): last_state(true), button_state(true), last_debounce_time(0){};
    bool debounce(uint button)
    {
        bool read_state = gpio_get(button);
        if (read_state != last_state)
        {
            last_debounce_time = xTaskGetTickCount();
        }
        if ((xTaskGetTickCount() - last_debounce_time) >= pdMS_TO_TICKS(10))
        {
            if (read_state != button_state)
            {
                button_state = read_state;
                if (button_state == false)
                {
                    return true;
                }
            }
        }
        last_state = read_state;
        return false;
    }
};
struct debugEvent
{
    const char *format;
    uint32_t data[3];
    uint32_t timestamp;
};

void debug(const char *format, uint32_t d1, uint32_t d2, uint32_t d3)
{
    debugEvent e;
    e.format = format;
    e.data[0] = d1;
    e.data[1] = d2;
    e.data[2] = d3;
    e.timestamp = xTaskGetTickCount();
    xQueueSend(syslog_q, &e, portMAX_DELAY);
}

void debugTask(void *pvParameters)
{
    char buffer[64];
    debugEvent e;

    while (true)
    {
        xQueueReceive(syslog_q, &e, portMAX_DELAY);
        snprintf(buffer, 64, e.format, e.data[0], e.data[1], e.data[2]);
        printf("[%lu] %s", e.timestamp, buffer);
        //std::cout << buffer ;
    }
}

void task1(void* parameter){
    ButtonState state;
    while(true)
    {
        while (!state.debounce(BUTTON1))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        while (gpio_get(BUTTON1) == false)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        xEventGroupSetBits(eventGroup, TASK1_BIT);
        debug("Button: %d pressed.\n", 1, 0, 0);
    }
}

void task2(void* parameter){
    ButtonState state;
    while(true)
    {
        while (!state.debounce(BUTTON2))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        while (gpio_get(BUTTON2) == false)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        xEventGroupSetBits(eventGroup, TASK2_BIT);
        debug("Button: %d pressed.\n", 2, 0, 0);
    }
}

void task3(void* parameter){
    ButtonState state;
    while(true)
    {
        while (!state.debounce(BUTTON3))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        while (gpio_get(BUTTON3) == false)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        xEventGroupSetBits(eventGroup, TASK3_BIT);
        debug("Button: %d pressed.\n", 3, 0, 0);
    }
}

void watchdog(void* parameter)
{
    TickType_t last_ok;
    TickType_t current_tick;
    uint32_t elapsed;
    last_ok = xTaskGetTickCount();
    while (true)
    {
        EventBits_t bits;
        bits = xEventGroupWaitBits(eventGroup,ALL_TASK_BITS, pdTRUE, pdTRUE, WATCHDOG_TIME );
        if ((bits & ALL_TASK_BITS) == ALL_TASK_BITS)
        {
            current_tick = xTaskGetTickCount();
            elapsed = current_tick - last_ok;
            last_ok = current_tick;
            debug("OK: %u ticks\n", elapsed, 0, 0);
        }else
        {
            if ((bits & TASK1_BIT) == 0)
            {
                debug("Fail: Task %u missed deadline\n", 1, 0, 0);
            }

            if ((bits & TASK2_BIT) == 0)
            {
                debug("Fail: Task %u missed deadline\n", 2, 0, 0);
            }

            if ((bits & TASK3_BIT) == 0)
            {
                debug("Fail: Task %u missed deadline\n", 3, 0, 0);
            }

            vTaskSuspend(nullptr);
        }
    }
}
int main()
{
    stdio_init_all();

    gpio_init(BUTTON1);
    gpio_set_dir(BUTTON1, GPIO_IN);
    gpio_pull_up(BUTTON1);

    gpio_init(BUTTON2);
    gpio_set_dir(BUTTON2, GPIO_IN);
    gpio_pull_up(BUTTON2);

    gpio_init(BUTTON3);
    gpio_set_dir(BUTTON3, GPIO_IN);
    gpio_pull_up(BUTTON3);

    syslog_q = xQueueCreate(10, sizeof(debugEvent));

    eventGroup = xEventGroupCreate();
    xTaskCreate(task1, "task 1", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(task2, "task 2", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(task3, "task 3", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(watchdog, "watchdog", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(debugTask, "debug", 256, nullptr, tskIDLE_PRIORITY + 1, nullptr);

    vTaskStartScheduler();
    while (true){};
}