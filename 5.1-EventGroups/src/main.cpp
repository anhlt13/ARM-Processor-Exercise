#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "queue.h"
#include "event_groups.h"
#include "cstdlib"

#define BUTTON 9
#define START_BIT 1 << 0


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

void task1(void *pvParameters)
{
    TickType_t last_print;
    TickType_t current_tick;
    uint32_t elapsed;
    uint32_t delay_time;
    ButtonState state;

    while (!state.debounce(BUTTON))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    xEventGroupSetBits(eventGroup, START_BIT);
    last_print = xTaskGetTickCount();
    srand(xTaskGetTickCount() + 1);
    while (true)
    {
        delay_time = 1000 + (rand() % 1001);
        vTaskDelay(pdMS_TO_TICKS(delay_time));
        current_tick = xTaskGetTickCount();
        elapsed = current_tick - last_print;
        last_print = current_tick;
        debug("Task %u: %u ticks\n",1, elapsed, 0 );
    }

}

void task2(void *pvParameters)
{
    TickType_t last_print;
    TickType_t current_tick;
    uint32_t elapsed;
    uint32_t delay_time;
    xEventGroupWaitBits(eventGroup, START_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    last_print = xTaskGetTickCount();
    srand(xTaskGetTickCount() + 2);
    while (true)
    {
        delay_time = 1000 + (rand() % 1001);
        vTaskDelay(pdMS_TO_TICKS(delay_time));
        current_tick = xTaskGetTickCount();
        elapsed = current_tick - last_print;
        last_print = current_tick;
        debug("Task %u: %u ticks\n",2, elapsed, 0 );
    }
}

void task3(void *pvParameters)
{
    TickType_t last_print;
    TickType_t current_tick;
    uint32_t elapsed;
    uint32_t delay_time;
    xEventGroupWaitBits(eventGroup, START_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    last_print = xTaskGetTickCount();
    srand(xTaskGetTickCount() + 3);
    while (true)
    {
        delay_time = 1000 + (rand() % 1001);
        vTaskDelay(pdMS_TO_TICKS(delay_time));
        current_tick = xTaskGetTickCount();
        elapsed = current_tick - last_print;
        last_print = current_tick;
        debug("Task %u: %u ticks\n",3, elapsed, 0 );

    }

}

int main()
{
    stdio_init_all();

    printf("\nBoot\n");
    gpio_init(BUTTON);
    gpio_set_dir(BUTTON, GPIO_IN);
    gpio_pull_up(BUTTON);

    syslog_q = xQueueCreate(10, sizeof(debugEvent));
    eventGroup = xEventGroupCreate();
    xTaskCreate(task1, "task1", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(task2, "task2", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(task3, "task3", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(debugTask,"debug task", 256, nullptr, tskIDLE_PRIORITY + 1, nullptr );
    vTaskStartScheduler();
    while (true){};
}