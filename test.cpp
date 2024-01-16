#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_EXAMPLE";

// MQTT configuration
static const char *mqtt_server = "mqtt.eclipse.org";
static const char *mqtt_topic = "example_topic";
static const int mqtt_port = 1883;

static esp_mqtt_client_handle_t mqtt_client;

// Callback to handle incoming MQTT messages
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(mqtt_client, mqtt_topic, 0);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_mqtt_client_config_t mqtt_cfg;
    mqtt_cfg.uri = "mqtt://mqtt.eclipse.org";
    mqtt_cfg.event_handle = mqtt_event_handler;
    // mqtt_cfg.username = "your_username";
    // mqtt_cfg.password = "your_password";

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
