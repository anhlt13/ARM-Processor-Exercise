#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "queue.h"

// this is needed for runtime statistics
#include "queue.h"
#include "hardware/timer.h"

//define led
#define BUTTON1 9
#define BUTTON2 8
#define BUTTON3 7

//define led
#define LED1 20
#define LED2 21
#define LED3 22

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

// change to using the object, so do not mixing between c and c++
class ButtonState
{
    private:
        bool last_state;
        bool button_state;
        TickType_t last_debounce_time;

    public:
        ButtonState(): last_state(true), button_state(true), last_debounce_time(0){};
        bool debounce(uint button_pin)
        {
            bool read_state = gpio_get(button_pin);
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

struct buttonTask{
    uint button_pin;
    QueueHandle_t queue;
};

void button_task(void* parameter)
{
    buttonTask* params = (buttonTask*) parameter;
    uint button_pin = params->button_pin;
    QueueHandle_t queue = params->queue;

    gpio_init(button_pin);
    gpio_set_dir(button_pin, GPIO_IN);
    gpio_pull_up(button_pin);

    ButtonState state ;
    bool run = true;
    while (run)

        if (state.debounce(button_pin))
        {
            uint button_number;
            if (button_pin == BUTTON1)
            {
                button_number = 0;
            }else if (button_pin == BUTTON2)
            {
                button_number = 1;
            }else
            {
                button_number = 2;
            }
            xQueueSend(queue, &button_number, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void processing_task(void* parameter)
{
    QueueHandle_t queue = (QueueHandle_t) parameter;
    const uint sequence[] = {0, 2, 1, 0, 2};
    uint sequence_position = 0;
    uint button;
    bool start = true;
    while (start)
    {
        if (xQueueReceive(queue, &button, pdMS_TO_TICKS(5000)) == pdTRUE)
        {
            if (button == 0)
            {
                gpio_put(LED1, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_put(LED1, 0);
            }else if (button == 1)
            {
                gpio_put(LED2, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_put(LED2, 0);
            }else
            {
                gpio_put(LED3, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_put(LED3, 0);
            }

            if (button == sequence[sequence_position])
            {
                sequence_position++;
                printf("Correct button - position = %d\n", sequence_position);
                if (sequence_position == 5)
                {
                    printf("LOCK OPEN!!\n");
                    gpio_put(LED1, 1);
                    gpio_put(LED2, 1);
                    gpio_put(LED3, 1);

                    if (xQueueReceive(queue, &button, pdMS_TO_TICKS(5000)) == pdTRUE)
                    {
                        printf("Button pressed - Lock closed\n");
                    }else
                    {
                        printf("5s passed - lock closed\n");
                    }
                    gpio_put(LED1, 0);
                    gpio_put(LED2, 0);
                    gpio_put(LED3, 0);
                    sequence_position = 0;
                }
            }else
            {
                printf("Wrong button - sequence reset\n");
                sequence_position = 0;
            }
        }else
        {
            printf("Timeout - sequence reset\n");
            sequence_position = 0;
        }
    }
}

int main()
{
    stdio_init_all();
    printf("\nBoot\n");

    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);
    gpio_put(LED1, 0);

    gpio_init(LED2);
    gpio_set_dir(LED2, GPIO_OUT);
    gpio_put(LED2, 0);

    gpio_init(LED3);
    gpio_set_dir(LED3, GPIO_OUT);
    gpio_put(LED3, 0);

    QueueHandle_t button_queue = xQueueCreate(10, sizeof(uint));
    if (button_queue == nullptr)
    {
        panic("Failed to create a queue");
    }

    buttonTask button1_params = {BUTTON1, button_queue};
    buttonTask button2_params = {BUTTON2, button_queue};
    buttonTask button3_params = {BUTTON3, button_queue};

    xTaskCreate(button_task, "Button 1", 512, &button1_params, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(button_task, "Button 2", 512, &button2_params, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(button_task, "Button 3", 512, &button3_params, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(processing_task, "Processing", 512, button_queue, tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();

    while(true){};
}

