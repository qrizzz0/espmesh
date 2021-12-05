#include "mesh.h"

void start_mesh() {
    if (meshActive) {
        return; //Jump out if the mesh is already active. We shouldn't be here in that case.
    }

    printf("Setting up WIFI\n");
    ESP_ERROR_CHECK(nvs_flash_init()); //Enables NVS flash needed for wifi mesh netif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default()); //Initialize event so we can have callbacks for the mesh
    if (gnetif == NULL) {
        ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&gnetif, NULL)); //This initializes wifi mesh and we save the pointer to the resulting netif in &gnetif
    }
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_callback, NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("Setting up mesh settings\n");
    //Set settings on mesh
    ESP_ERROR_CHECK(esp_mesh_init());
    if (SETTING_MESH_ROOT) {
        ESP_ERROR_CHECK(esp_mesh_set_type(MESH_ROOT));
    } else {
        ESP_ERROR_CHECK(esp_mesh_set_type(MESH_IDLE));
        ESP_ERROR_CHECK(esp_mesh_fix_root(true)); 
    }
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_callback, NULL)); //This makes a callback on mesh events to the function 
    ESP_ERROR_CHECK(esp_mesh_set_topology(MESH_TOPO_TREE)); //Sets the mesh topology to either tree or chain. We do tree as we have a single point that all packets should end at, but structurally we do not want to chain.
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(25)); //We go all in and allow 25 hops on our tree (which is max). Let's see in future what is best.
    ESP_ERROR_CHECK(esp_mesh_set_xon_qsize(64)); //Set a pretty big message queue
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mesh_disable_ps());

    //This might also be needed:
    ESP_ERROR_CHECK(esp_mesh_allow_root_conflicts(false)); //We want 1 and only 1 root
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(WIFI_AUTH_OPEN));

    printf("Setting mesh config\n");
    //Create a new mesh config with params we want
    mesh_cfg_t mesh_config = MESH_INIT_CONFIG_DEFAULT();
    memcpy((uint8_t *) &mesh_config.mesh_id, MESH_ID, 6); //Put in mesh id
    mesh_config.channel = 0; //set wifi channel for mesh
    mesh_config.mesh_ap.max_connection = 10; //Some high number of connections, max is 10 I believe
    mesh_config.router.ssid_len = strlen(WIFI_SSID);
    memcpy(mesh_config.router.ssid, WIFI_SSID, mesh_config.router.ssid_len);
    memcpy(mesh_config.router.password, WIFI_PASSWORD, strlen(WIFI_PASSWORD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&mesh_config));

    printf("Starting mesh\n");
    ESP_ERROR_CHECK(esp_mesh_start());

    printf("Changing mesh to self organized\n");
    ESP_ERROR_CHECK(esp_mesh_set_self_organized(true, false)); //Set mesh to self organized (true)

    meshActive = true;

    if (!SETTING_MESH_ROOT) {
        xTaskCreate(resendToRootTask, "resend task", 2048, NULL, 0, NULL);
    }

    esp_read_mac(this_device_mac_addr, 0);
    printf("This device has MAC address: %x:%x:%x:%x:%x:%x\n", this_device_mac_addr[0], this_device_mac_addr[1], this_device_mac_addr[2], this_device_mac_addr[3], this_device_mac_addr[4], this_device_mac_addr[5]);

    xTaskCreate(receiveTask, "receive task", 4096, NULL, 0, NULL);
}

void stop_mesh() {
    meshActive = false;
    printf("Waiting for %d seconds before progressing sleep\n", MESH_SLEEP_RETRY_ATTEMPTS * MESH_SLEEP_RETRY_WAIT);
    vTaskDelay(MESH_SLEEP_RETRY_ATTEMPTS * MESH_SLEEP_RETRY_WAIT * SECOND);

    ESP_ERROR_CHECK(esp_mesh_set_self_organized(false, false)); //Reset self-organized in mesh just in case
    ESP_ERROR_CHECK(esp_mesh_stop()); //Stop mesh
    ESP_ERROR_CHECK(esp_wifi_stop()); //Stop WIFI
}

void root_broadcast(uint8_t* data, uint32_t length) {
    static mesh_addr_t routes[MAX_TOTAL_CHILDREN];
    int route_table_length = 0;
    mesh_data_t root_announce_data;
    root_announce_data.proto = MESH_PROTO_BIN;
    root_announce_data.tos = MESH_TOS_P2P;
    root_announce_data.data = data;
    root_announce_data.size = length;

    int retries = 0;
    do {
        esp_mesh_get_routing_table(routes, MAX_TOTAL_CHILDREN, &route_table_length);

        for (int i = 0; i < route_table_length; i++) {
            if (memcmp(routes[i].addr, this_device_mac_addr, 6) != 0) {
                esp_mesh_send(&routes[i], &root_announce_data, MESH_DATA_P2P, NULL, 0);
            }
        }

        vTaskDelay(MESH_SLEEP_RETRY_WAIT * SECOND); //Wait 5 seconds
        retries++;
    } while (retries < MESH_SLEEP_RETRY_ATTEMPTS && childCount > 0);
}

void resendToRootTask(void* unused) {
    outQueue = xQueueCreate(20, sizeof(mesh_data_t));

    mesh_data_t currentPackage;

    while (meshActive) {
        BaseType_t somethingToSend = xQueueReceive(outQueue, &currentPackage, 5 * SECOND); //Let this timeout so we can close task if we close mesh
        if (somethingToSend == pdTRUE) {
            printf("Resending a pakcage!\n");
            esp_err_t status = 1;
            while (status != ESP_OK) {
                status = esp_mesh_send(NULL, &currentPackage, MESH_DATA_P2P, NULL, 0); //If this fails we keep trying until it works.
            }
            free(currentPackage.data);
        }
    }

    while(xQueueReceive(outQueue, &currentPackage, 0) == pdTRUE) {
        //At this point, we should just free all our memory if there's anything left
        free(currentPackage.data);
    }

    vQueueDelete(outQueue);
    vTaskDelete(NULL);
}

void receiveTask(void* unused) {
    mesh_data_t dataBuffer;
    mesh_addr_t sender;
    uint8_t* recieve_buffer = malloc(RX_SIZE * sizeof(uint8_t));
    dataBuffer.data = recieve_buffer;
    dataBuffer.size = RX_SIZE;

    while(meshActive) {
        esp_err_t status = esp_mesh_recv(&sender, &dataBuffer, 5 * SECOND, 0, NULL, 0); //We timeout after 5 seconds so we can check meshActive and jump out of the task.
        if ((status != ESP_OK && status != ESP_ERR_MESH_TIMEOUT) || !dataBuffer.size) {
            printf("error in receiving package: %s, size: %d\n", esp_err_to_name(status), dataBuffer.size);
        } else if (status == ESP_OK) {
            printf("received data from mesh: %s\n", dataBuffer.data);
            processPacket(dataBuffer.data, dataBuffer.size);
        }
    }

    free(recieve_buffer);
    vTaskDelete(NULL);
}

void sendDataToRoot(uint8_t* data, uint32_t length) {
    if (meshActive == false) {
        return; //Fail out immediately if mesh is down
    }

    mesh_data_t meshData;
    meshData.proto = MESH_PROTO_BIN;
    meshData.tos = MESH_TOS_P2P; //Send without retransmissions, just go!
    uint8_t* dataToSend = malloc(length * sizeof(uint8_t));
    memcpy(dataToSend, data, length * sizeof(uint8_t));

    meshData.data = dataToSend;
    meshData.size = length;

    esp_err_t status = esp_mesh_send(NULL, &meshData, 0, NULL, 0); //Third argument is 0 when sending to root: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp-wifi-mesh.html#mesh-application-examples
    if (status != ESP_OK && outQueue != NULL) {
        xQueueSend(outQueue, (void*) &meshData, 0); //xQueueSend copies, does not reference, even though it is a pointer.
    } else {
        free(meshData.data);
    }
}

void mesh_callback(void* arguments, esp_event_base_t event, int32_t eventId, void* eventData) {
    switch (eventId) {
        case MESH_EVENT_STARTED:
            printf("Mesh started\n");
            break;
        case MESH_EVENT_STOPPED:
            printf("Mesh stopped\n");
            break;
        case MESH_EVENT_CHILD_CONNECTED: 
            printf("Mesh child connected event\n");
            mesh_event_child_connected_t* child = (mesh_event_child_connected_t*) eventData;
            printf("Child id: %x:%x:%x:%x:%x:%x\n", child->mac[0], child->mac[1], child->mac[2], child->mac[3], child->mac[4], child->mac[5]);
            childCount++;
            printf("Total children: %d\n", childCount);
            break;
        case MESH_EVENT_CHILD_DISCONNECTED:
            printf("Mesh child disconnected event");
            //mesh_event_child_disconnected_t* child_dc = (mesh_event_child_disconnected_t*) eventData; //If we need to get dcing child at some point this is how
            childCount--;
            printf("Total children: %d\n", childCount);
            break;
        case MESH_EVENT_PARENT_CONNECTED: 
            printf("Connected to a parent\n");
            mesh_layer = esp_mesh_get_layer();
            printf("My layer is: %d\n", mesh_layer);
            if (mesh_layer == 2) {
                gpio_set_level(2,1);
            }

            //When we are connected to parent, we can start DHCP. The root parent is the WIFI Router
            if (SETTING_MESH_ROOT) {
                esp_err_t status = esp_netif_dhcpc_start(gnetif); //Start dhcp client on root
                printf("Enabled DHCP and got status: %d", status);
            }
            break;
            
        default:
            //printf("Unhandled mesh event: %d\n", eventId);
            break;
    }

}

void ip_callback(void* arguments, esp_event_base_t eventBase, int32_t eventID, void *eventPtr) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) eventPtr;
    printf("IP Callback IP: %u\n", event->ip_info.ip.addr);
    mqtt_start(); //When we have IP we can start mqtt
}

void processPacket(uint8_t* data, uint32_t length) {
    if (length < 5 || length > 1500) {
        return; //Something is very wrong with the packet, return immediately
    }

    if (memcmp("SLEEP:", data, 6) == 0) {
        //Alright, lets sleep!
        int sleepTime = atoi((char*)data + 6);
        mesh_sleep(sleepTime);
    }

    if (memcmp("DATA:", data, 5) == 0) {
        char* dataPtr = (char*) data + 5;
        int outValue = strtol(dataPtr, &dataPtr, 10);
        printf("Received data packet with data: %d   -   ", outValue);

        if (memcmp(" ID:", dataPtr, 4) == 0) { //Might as well keep checking package validity
            dataPtr += 4;
            int sensorID = strtol(dataPtr, NULL, 10);
            printf("And ID: %d\n", sensorID);
            mqtt_publish(MQTT_DATA_TOPIC, (char*) data);
        }
    }
}

void mesh_sleep(int seconds) {
    //When we receive the signal to sleep, we wait for the root retry-attempts time before shutting down.
    //Waiting for that time guarantees that nodes connected to us have a chance of receiving the sleep signal as well.
    printf("Stopping and sleeping for %d seconds\n", seconds);
    stop_mesh();
    vTaskDelay(10 * SECOND); //Give some time to let everything shut down nicely
    esp_deep_sleep(1000000LL * seconds);
}

bool mesh_running() {
    return meshActive;
}