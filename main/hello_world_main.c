/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "mesh.h"
#include "settings.h"
#include "adc_sensor.h"
#include "mqtt.h"

void awake_timer_task(void* unused) {
    vTaskDelay(SECONDS_AWAKE * SECOND); //Wait for SECONDS AWAKE before doing anything

    printf("Time's up, putting everything to sleep!\n");

    char sleepCommand[100];
    int sleepCommandLength = sprintf(sleepCommand, "SLEEP:%d", SLEEP_TIME);
    sleepCommand[sleepCommandLength] = '\0'; //Add string terminator
    sleepCommandLength += 1;
   
    root_broadcast((uint8_t*) sleepCommand, sleepCommandLength);
    //When we get out of root_broadcast all other devices are sleeping, unless they are broken or too far away, in that case it is not our problem.

    mesh_sleep(SLEEP_TIME);
}

void app_main(void)
{
    /*printf("Hello world!\n");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());*/

    if (SETTING_MESH_ROOT) {
        xTaskCreate(awake_timer_task, "awake timer", 4096, NULL, 0, NULL);
    }
    
    printf("running esp mesh function\n");
    start_mesh();

    while (!mesh_running()) {
        vTaskDelay(SECOND); //Wait for mesh to be up
    }

    //After mesh is up, start sensor
    if (SETTING_HASSENSOR) {
        printf("Starting ADCTask\n");
        xTaskCreate(ADCtask, "adc task", 2048, NULL, 0, NULL);
    }
}