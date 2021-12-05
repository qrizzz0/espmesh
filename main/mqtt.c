#include "mqtt.h"

void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
            .uri = MQTT_ADDRESS,
    };

    clientHandle = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(clientHandle, ESP_EVENT_ANY_ID, mqttEventHandler, clientHandle);
    esp_mqtt_client_start(clientHandle);
}

void mqttEventHandler(void* args, esp_event_base_t eventBase, int32_t event_id, void* eventPtr) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) eventPtr;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            printf("MQTT_EVENT_CONNECTED\n");
            if (esp_mqtt_client_subscribe(clientHandle, "/topic/hej", 0) < 0) {
                // Disconnect to retry the subscribe after auto-reconnect timeout
                esp_mqtt_client_disconnect(clientHandle);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            printf("MQTT_EVENT_DISCONNECTED\n");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            printf("MQTT_EVENT_SUBSCRIBED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            printf("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            printf("MQTT_EVENT_PUBLISHED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            printf("MQTT_EVENT_DATA\n");
            break;
        case MQTT_EVENT_ERROR:
            printf("MQTT_EVENT_ERROR\n");
            break;
        default:
            printf("Other event id:%d\n", event->event_id);
            break;
    }
    return ESP_OK;
}

void mqtt_publish(char* topic, char* data)
{
    printf("Attempting to send MQTT package, topic: %s, data: %s\n", topic, data);
    mqttMsg* msg = malloc(sizeof(mqttMsg));
    if (clientHandle != NULL) {
        while(xQueueReceive(mqttQueue, msg, 0) == pdTRUE) {
            esp_mqtt_client_publish(clientHandle, msg->topic, msg->data, 0, 1, 0); //QoS 1, so we make sure data arrived
        }

        esp_mqtt_client_publish(clientHandle, topic, data, 0, 1, 0); //QoS 1, so we make sure data arrived
    } else {
        if (mqttQueue == NULL) {
            mqttQueue = xQueueCreate(50, RX_SIZE + 50); //50 packages should be 1550 * 50 = 75K ram which is under 1/4 of ESP and should be possible and still quite a few readings
        }
        strcpy(msg->data, data);
        strcpy(msg->topic, topic);
        xQueueSend(mqttQueue, msg, 0);
    }

    free(msg);
}