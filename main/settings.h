#ifndef SETTINGS_H
#define SETTINGS_H

#define SETTING_HASSENSOR true
#define SETTING_ESPID 53

//These options are controlled entirely on the root device. 
#define SETTING_MESH_ROOT true
#define WIFI_SSID "Asuna"
#define WIFI_PASSWORD "xxx"
#define MQTT_ADDRESS "mqtt://192.168.240.142"
#define MQTT_DATA_TOPIC "/topic/mesh_data"

#define SECONDS_AWAKE 30 //We are awake for 2 minutes before we shut down again. This should be enough time to wake everyone up and sample. Can be changed at will.
#define SLEEP_TIME 60 //This is our variable to adjust the sleep to be however long we want!
#define MAX_TOTAL_CHILDREN  255 //The maximum amount of children in the network

//These should be the same for all devices
#define MESH_SLEEP_RETRY_ATTEMPTS 5
#define MESH_SLEEP_RETRY_WAIT 5 //How many seconds to wait between retries
#define RX_SIZE             (1500)
#define TX_SIZE             (1460)

static const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77}; //The ID of the mesh

#endif //SETTINGS_H