#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "semphr.h"

// this is needed for runtime statistics
#include "hardware/timer.h"

// define led
#define LED1 20
#define LED2 21
#define LED3 22
SemaphoreHandle_t semaphore;

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

void serial_task(void*)
{
    int character;
    while (true)
    {
        character = getchar_timeout_us(0);
        if (character != PICO_ERROR_TIMEOUT)
        {
            putchar(character);
            xSemaphoreGive(semaphore);
        }
        else vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void blink_led(uint led1, uint led2, uint led3)
{
    gpio_put(led1, 1);
    gpio_put(led2, 1);
    gpio_put(led3, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_put(led1, 0);
    gpio_put(led2, 0);
    gpio_put(led3, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

}

void blink_task (void* )
{
    while (true)
    {
       if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE)
       {
           blink_led(LED1, LED2, LED3);
       }
    }
}


int main()
{
    stdio_init_all();
    printf("\nBoot\n");
    semaphore = xSemaphoreCreateBinary();
    if (semaphore == nullptr)
    {
        printf("Semaphore creation failed\n");
        while (true){};
    }
    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);
    gpio_put(LED1, 0);


    gpio_init(LED2);
    gpio_set_dir(LED2, GPIO_OUT);
    gpio_put(LED2, 0);

    gpio_init(LED3);
    gpio_set_dir(LED3, GPIO_OUT);
    gpio_put(LED3, 0);

    xTaskCreate(serial_task, "serial task", 512, NULL, 1, nullptr);
    xTaskCreate(blink_task, "blink task", 512, NULL, tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();

    while(true){};
}

