#pragma once 

#include <stdio.h>
#include "settings.h"
#include "mqtt_client.h"

static esp_mqtt_client_handle_t clientHandle = NULL;
static QueueHandle_t mqttQueue = NULL; 

typedef struct {
    char topic[50];
    char data[1500];
} mqttMsg;

void mqtt_start();
void mqttEventHandler(void* args, esp_event_base_t eventBase, int32_t event_id, void* eventPtr);
void mqtt_publish(char* topic, char* data);