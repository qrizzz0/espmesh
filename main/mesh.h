#ifndef MESH_H
#define MESH_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mesh.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "settings.h"
#include "mqtt.h"
#define SECOND              1000 / portTICK_PERIOD_MS

//Global vars:
static esp_netif_t *gnetif = NULL;
uint8_t this_device_mac_addr[6];
static int mesh_layer = 0;
static int childCount = 0;
static bool meshActive = false;
static QueueHandle_t outQueue = NULL;

//Function prototypes
void start_mesh();
void stop_mesh();
void root_broadcast(uint8_t* data, uint32_t length);
void resendToRootTask(void* unused);
void receiveTask(void* unused);
void sendDataToRoot(uint8_t* data, uint32_t length);
void mesh_callback(void* arguments, esp_event_base_t event, int32_t eventId, void* eventData);
void ip_callback(void* arguments, esp_event_base_t eventBase, int32_t eventID, void *event);
void processPacket(uint8_t* data, uint32_t length); //Currently we only process sleep packets and sensors packets as these are all we need for our system.
void mesh_sleep(int seconds);
bool mesh_running();

#endif /* MESH_H */