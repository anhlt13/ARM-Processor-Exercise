#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "queue.h"


#define ROTA 10
#define ROTB 11
#define ROTSW 12

#define LED 20
#define LED_ON false
#define FREQUENCY 2

// this is needed for runtime statistics
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

typedef enum
{
    BUTTON_EVENT,
    ENCODER_CW,
    ENCODER_CCW,
} gpio_event_t;

typedef struct
{
    bool led_on;
    int frequency;
} led_state_t;


led_state_t led_state = {LED_ON, FREQUENCY};
QueueHandle_t gpio_queue;

void gpio_callback(uint gpio, uint32_t events)
{
    gpio_event_t event;
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    if (gpio == ROTA)
    {
        if (gpio_get(ROTB) == 0)
        {
            event = ENCODER_CW;
        }else
        {
            event = ENCODER_CCW;
        }
        xQueueSendFromISR(gpio_queue, &event, &pxHigherPriorityTaskWoken);
    }else if (gpio == ROTSW)
    {
        event = BUTTON_EVENT;
        xQueueSendFromISR(gpio_queue, &event, &pxHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void gpio_task(void* parameter)
{
    gpio_event_t event;
    TickType_t last_button_time = 0;


    while (true)
    {
        if (xQueueReceive(gpio_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            if (event == BUTTON_EVENT)
            {
                TickType_t now = xTaskGetTickCount();
                if (now - last_button_time >= pdMS_TO_TICKS(250))
                {
                    led_state.led_on = ! led_state.led_on;
                    if (led_state.led_on)
                    {
                        printf("LED on, frequency = %d Hz\n", led_state.frequency);
                    }else
                    {
                        printf("LED off, frequency = %d Hz\n",led_state.frequency);
                    }
                    last_button_time = now;
                }
               // printf("Button pressed\n");
            }else if (event == ENCODER_CW)
            {
                if (led_state.led_on)
                {
                    if (led_state.frequency < 200)
                    {
                        led_state.frequency ++;
                    }
                    printf("Frequency: %d\n", led_state.frequency);
                }
                //printf("Turning clockwise\n");
            }else if (event == ENCODER_CCW)
            {
                if (led_state.led_on)
                {
                    if (led_state.frequency > 2)
                    {
                        led_state.frequency--;
                    }
                    printf("Frequency: %d\n", led_state.frequency);
                }
                //printf("Turning counterclockwise\n");
            }
        }
    }
}

void blink_task(void* parameter)
{

    while (true)
    {
        if (led_state.led_on)
        {
            int half_period = 1000 / led_state.frequency / 2;
            gpio_put(LED, 1);
            vTaskDelay(pdMS_TO_TICKS(half_period));
            gpio_put(LED, 0);
            vTaskDelay(pdMS_TO_TICKS(half_period));
        }else
        {
            gpio_put(LED, 0);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

int main()
{
    stdio_init_all();
    printf("BOOT\n");

    gpio_init(ROTA);
    gpio_set_dir(ROTA, GPIO_IN);

    gpio_init(ROTB);
    gpio_set_dir(ROTB, GPIO_IN);

    gpio_init(ROTSW);
    gpio_set_dir(ROTSW, GPIO_IN);
    gpio_pull_up(ROTSW);

    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
    gpio_put(LED, 0);


    gpio_queue = xQueueCreate(10, sizeof(gpio_event_t));
    vQueueAddToRegistry(gpio_queue, "GPIO Queue");

    gpio_set_irq_enabled_with_callback(ROTA, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(ROTSW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    xTaskCreate(gpio_task, "GPIO Task", 256, &led_state, tskIDLE_PRIORITY + 1 , NULL);
    xTaskCreate(blink_task, "Blink Task", 256, &led_state, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
    while (1){};

}
