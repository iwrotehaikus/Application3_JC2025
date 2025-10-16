/* --------------------------------------------------------------
   Application: 03 - Rev1
   Class: Real Time Systems - Fa 2025
   Author: [John Crawford] 
   Email: [evancrawford@ucf.edu]
   Company: [University of Central Florida]
   AI Use: Commented inline -- None
---------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

// BONUS Adding in some examples of using the built in ESP Logging capabilties!
const static char *TAG = "SAT-03>";
#define CONFIG_LOG_DEFAULT_LEVEL_VERBOSE  (1)
#define CONFIG_LOG_DEFAULT_LEVEL          (5)
#define CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE  (1)
#define CONFIG_LOG_MAXIMUM_LEVEL          (5)
#define CONFIG_LOG_COLORS                 (1)
#define CONFIG_LOG_TIMESTAMP_SOURCE_RTOS  (1) 
// You can safely ignore these lines for now

// Pins
#define LED_PIN GPIO_NUM_2  // Using GPIO2 for the LED -- not connected in the code for simplicity
#define BUTTON_GPIO GPIO_NUM_4
#define LOG_BUFFER_SIZE 50

// Shared buffer and index
static uint16_t sensor_log[LOG_BUFFER_SIZE];
static int log_index = 0;

// Synchronization
static SemaphoreHandle_t xLogMutex;  // Hand off access to the buffer!
static SemaphoreHandle_t xButtonSem;  // Hand off on button press!

// === ISR: Triggered on button press ===
void IRAM_ATTR button_isr_handler(void *arg) {
    ESP_LOGV(TAG,"button pressed - setting semaphore to be taken by logger \n");
    BaseType_t xHigherPrioTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPrioTaskWoken);
    portYIELD_FROM_ISR(xHigherPrioTaskWoken);
}
// Task: LED Blink Task (Sattelite Status)
void led_blink_task(void *arg) {
    
    while (1) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(2500);
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(2500);
    }
}

//Task: Print to console (Sattelite Comms)
void console_task(void *arg) {
    while (1) {
        ESP_LOGI(TAG, "Telemetry: Satellite Operational, buffer idx=%d", log_index);
        vTaskDelay(7000);
    }
}

// Task: Read sensor and store in buffer 
void sensor_task(void *arg) {
    ESP_LOGV(TAG,"sensor entered \n");
    while (1) {
   
        // Simulate reading light sensor value; See application 2 to do that
        uint16_t val = xTaskGetTickCount() % 4096; 

        // Lock mutex to write value to buffer  || What happens if you lock it for too short or long of a time? :)
        if (xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(10))) {
           ESP_LOGV(TAG,"sensor taking log data semaphore for write\n");
            sensor_log[log_index] = val;
            // this circular buffer will have issues at startup and when cleared; no clearing logic here
            log_index = (log_index + 1) % LOG_BUFFER_SIZE; 
            xSemaphoreGive(xLogMutex);  //give up the semaphore!
           ESP_LOGV(TAG,"sensor releasing log data semaphore\n");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
// Task: Wait for button, "compress" and dump log
void logger_task(void *arg) {
    while (1) {
        // Block until ISR signals button press
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGV(TAG,"logger entered based on button semaphore; will release on exit\n");
            uint16_t copy[LOG_BUFFER_SIZE];
            int count = 0;

            // Lock mutex to read from buffer
            if (xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(100))) {
                ESP_LOGV(TAG,"logger took log data semaphore for copy\n");
                memcpy(copy, sensor_log, sizeof(copy));
                count = log_index;
                xSemaphoreGive(xLogMutex);
                ESP_LOGV(TAG,"logger releasing log data semaphore\n");
            }

            // Simulate compression (calculate stats)
            uint16_t min = 4095, max = 0;
            uint32_t sum = 0;
            for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
                if (copy[i] < min) min = copy[i];
                if (copy[i] > max) max = copy[i];
                sum += copy[i];
            }

            uint16_t avg = sum / LOG_BUFFER_SIZE;
            ESP_LOGI(TAG, "[LOG DUMP] readings %d: min=%d, max=%d, avg=%d", LOG_BUFFER_SIZE, min, max, avg);
            ESP_LOGI(TAG, "Log transmission complete. Awaiting next command from Mission Control.\n");

            // consider - this code doesn't really "dump" the buffer does it?
        }
    }
}

// === Main Setup ===
void app_main(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Create synchronization primitives
    xLogMutex = xSemaphoreCreateMutex();
    xButtonSem = xSemaphoreCreateBinary();
    assert(xLogMutex && xButtonSem);


    gpio_install_isr_service(0);

    ESP_LOGI(TAG,"configuring LED\n");
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG,"configuring button\n");
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_GPIO);
    gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    
    // Create tasks
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 2048, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(logger_task, "logger_task", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(led_blink_task, "LED_Blink", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(console_task, "Console", 4096, NULL, 1, NULL, 1);

    ESP_LOGI(TAG, "System ready. Press the button to dump the log.");
}

