#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#define BUTTON1 7
#define BUTTON2 8
#define BUTTON3 9

#define LED1 20
#define LED2 21
#define LED3 22

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
}}

struct buttonState
{
    bool last_state;
    bool button_state;
    TickType_t last_debounce_time;
};

bool debounceButton(uint button_pin, buttonState& button_state)
{
    bool read_state = gpio_get(button_pin);
    if (read_state != button_state.last_state)
    {
        button_state.last_debounce_time = xTaskGetTickCount();
    }
    if ((xTaskGetTickCount() - button_state.last_debounce_time) >= pdMS_TO_TICKS(10))
    {
        if (read_state != button_state.button_state)
        {
            button_state.button_state = read_state;
            if (button_state.button_state == false)
            {
                button_state.button_state = false;
                return true;
            }
        }
    }
    button_state.last_state = read_state;
    return false;
}

void blinkLed(void* parameter)
{
    const uint button_pin = (uint) parameter;
    uint led_pin;
    uint interval;

    const char *button_name;
    const char *led_name;

    if (button_pin == BUTTON1)
    {
        led_pin = LED1;
        interval = 100;
        button_name = "BUTTON1";
        led_name = "LED1";
    }else if (button_pin == BUTTON2)
    {
        led_pin = LED2;
        interval = 300;
        button_name = "BUTTON2";
        led_name = "LED2";
    }else
    {
        led_pin = LED3;
        interval = 500;
        button_name = "BUTTON3";
        led_name = "LED3";
    }
    printf("%s - %s - interval = %d\n",  button_name, led_name, interval);

    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    gpio_put(led_pin, 0);

    gpio_init(button_pin);
    gpio_set_dir(button_pin, GPIO_IN);
    gpio_pull_up(button_pin);

    buttonState buttonState = {
        .last_state = true,
        .button_state = true,
        .last_debounce_time = 0
    };

    bool run = true;
    bool led_state = false;
    TickType_t last_wake_time = xTaskGetTickCount();
    while (run)
    {
        if (debounceButton(button_pin, buttonState))
        {
            if (interval == 500)
            {
                interval = 0;
            } else if (interval == 0)
            {
                interval = 100;
            }else
            {
                interval += 100;
            }

            printf("[DEBUG] %s pressed -> %s -> interval = %u ms\n",button_name,led_name,interval);
        }
        if (interval == 0)
        {
            led_state = false;
            gpio_put(led_pin, 0);
        }else
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_wake_time) >= pdMS_TO_TICKS(interval))
            {
                led_state = !led_state;
                gpio_put(led_pin, led_state);
                last_wake_time = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

}


int main()
{
   stdio_init_all();

    printf("\nBOOT\n");
    xTaskCreate(blinkLed,"BUTTON1",256,(void*)BUTTON1,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(blinkLed,"BUTTON2",256,(void*)BUTTON2,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(blinkLed,"BUTTON3",256,(void*)BUTTON3,tskIDLE_PRIORITY + 1,NULL);

    vTaskStartScheduler();

    while(true){};
}

