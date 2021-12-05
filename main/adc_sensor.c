#include "adc_sensor.h"

void ADCtask(void *pvParameters) {
    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    
    //Continuously sample ADC1
    while (mesh_running()) {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            }
        }
        adc_reading = adc_reading / NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
        
        char meshData[100];

        int dataLen = sprintf(meshData, "DATA:%d ID:%d", voltage, SETTING_ESPID);
        dataLen += 1;
        meshData[dataLen] = '\0';
  
        sendDataToRoot((uint8_t*) meshData, dataLen); //Send reading out on mesh network.

        vTaskDelay(5 * SECOND);
    }

    vTaskDelete(NULL);
}