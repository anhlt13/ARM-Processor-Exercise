#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "PicoOsUart.h"
#include "semphr.h"

#define led 20
#define inactivity_time pdMS_TO_TICKS(30000)
#define default_interval 5
SemaphoreHandle_t semaphore;


// this is needed for runtime statistics
#include <cstring>

#include "timers.h"
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

//Timers
PicoOsUart *uart;
TimerHandle_t inactivityTimer;
TimerHandle_t ledTimer;
char command_buffer[64];
int command_index = 0;
int ledInterval = default_interval;
TickType_t last_toggle_time;


void inactivity_time_callback(TimerHandle_t xTimer)
{
    //printf("DEBUG: Inactivity timeout at %lu seconds\r\n",(unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ));
    xSemaphoreGive(semaphore);
}

void led_time_callback(TimerHandle_t xTimer)
{
    gpio_put(led, !gpio_get(led));
    last_toggle_time = xTaskGetTickCount();
}

void print_help()
{
    uart->send("help - display usage instructions\r\n");
    uart->send("interval <number> - set the led toggle interval\r\n");
    uart->send("time - prints the number of seconds since the last led toggle\r\n");
}

void progress_command()
{
    command_buffer[command_index] = '\0';
    if (strcmp(command_buffer, "help") == 0)
    {
        print_help();
    }else if (strcmp(command_buffer, "time") == 0)
    {
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed_tick = now - last_toggle_time;
        //float elapsed_time = (float)elapsed_tick / configTICK_RATE_HZ;
        uint32_t elapsed_time = (elapsed_tick*10) / configTICK_RATE_HZ;
        char message[64];
        //snprintf(message, sizeof(message), "%.2f seconds \r\n", elapsed_time);
        sprintf(message, "elapsed time: %lu.%lu seconds\r\n", (unsigned long)(elapsed_time/10),  (unsigned long)(elapsed_time%10));
        uart->send(message);


    }else if (strncmp(command_buffer, "interval ", 9) == 0)
    {
        int new_interval;
        if (sscanf(command_buffer + 9, "%d", &new_interval) == 1)
        {
            if (new_interval > 0)
            {
                ledInterval = new_interval;
                xTimerChangePeriod(ledTimer, pdMS_TO_TICKS(ledInterval *1000), pdMS_TO_TICKS(100));
                uart->send("interval changed\r\n");
            }else
            {
                uart->send("unknown command\r\n");
            }
        }
    }else
    {
        uart->send("unknown command\r\n");
    }
    command_index = 0;
    command_buffer[0] = '\0';
}

void serial_task(void* parameter)
{
    PicoOsUart u(0,0,1,115200);
    uart = &u;
    uint8_t character;
    while (true)
    {
        if (xSemaphoreTake(semaphore, 0)== pdTRUE)
        {
            command_index = 0;
            command_buffer[0] = '\0';
            uart->send("[Inactive]\r\n");
        }

        int read =  u.read(&character, 1, 100);
        if (read > 0)
        {
            xTimerReset(inactivityTimer, pdMS_TO_TICKS(100));
            if (character == '\r' || character == '\n')
            {
                if (command_index > 0)
                {
                    printf("\r\n");
                    progress_command();
                }
            }else
            {
                if (command_index < sizeof(command_buffer)-1)
                {
                    command_buffer[command_index] = (char)character;
                    command_index++;
                    u.write(&character, 1);
                }
            }
        }
    }
}

int main ()
{
    stdio_init_all();
    printf("Boot\n");

    gpio_init(led);
    gpio_set_dir(led, GPIO_OUT);
    gpio_put(led, 0);

    semaphore = xSemaphoreCreateBinary();
    if (semaphore == nullptr)
    {
        printf("Semaphore creation failed\n");
        while (true){};
    }
    inactivityTimer = xTimerCreate("Inactivity", inactivity_time, pdFALSE, nullptr, inactivity_time_callback);
    ledTimer = xTimerCreate("LED", pdMS_TO_TICKS(default_interval * 1000), pdTRUE, nullptr, led_time_callback);
    xTimerStart(ledTimer, pdMS_TO_TICKS(100));
    xTaskCreate(serial_task, "serial", 512, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();
    while (true){}
}