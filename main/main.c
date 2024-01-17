
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_timer.h"  // Include the ESP timer header
#include "esp_sleep.h"

//#include "esp_netif.h" //koli NIC
//#include "protocol_examples_common.h" //koli examples
#include "esp_wifi.h" //wifi onlu

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h" // y wifi?

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"

#include "mqtt_client.h"

#include "driver/gpio.h"
#include "led_strip.h"

#define ESP_WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS       CONFIG_ESP_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY   CONFIG_ESP_MAXIMUM_RETRY
#define ESP_MQTT_HOST_URI   CONFIG_ESP_MQTT_HOST_URI

#define LED_SHIELD_GPIO     CONFIG_ESP_LED_PIN //CONFIG_LED_SHIELD_GPIO
#define LED_SHIELD_LENGHT   CONFIG_ESP_LED_PIN_LEN //CONFIG_LED_SHIELD_GPIO

#define CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_SPI true //force to spi //rmt is beter on 2 cores
#define CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_RMT false
#define CONFIG_INTERNAL_LED_PERIOD 1000

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_FAIL_BIT       ( 1 << 0 ) //each BIT like 0000
#define WIFI_CONNECTED_BIT  ( 1 << 1 ) //each BIT like 0010
//this means that you set new bit and clear others...

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "ESP";

static int s_retry_num = 0;

// Define the enumeration
typedef enum {
    S_WIFI_DISCONNECTED,
    S_WIFI_CONNECTING,
    S_WIFI_CONNECTED
    // Add more states as needed
} WifiState;

// Define the enumeration
typedef enum {
    S_MQTT_DISCONNECTED,
    S_MQTT_CONNECTING,
    S_MQTT_CONNECTED
    // Add more states as needed
} MQTTState;

// Define the enumeration
typedef enum {
    S_BLE_DISCONNECTED,
    S_BLE_CONNECTING,
    S_BLE_CONNECTED
    // Add more states as needed
} BLEState;

//signaling color
int luminance = 255; //at full
int black[3] = {255, 255, 255};
int deep[3] = {10, 10, 10};
int gray[3] = {50, 60, 40};
int red[3] = {255, 0, 0};
int green[3] = {0, 255, 0};
int blue[3] = {0, 0, 255};
int orange[3] = {255, 100, 0};
int white[3] = {255, 255, 255};
int violet[3] = {255, 0, 255};
int pink[3] = {255,50,250};
int yellow[3] = {255, 200, 0};

WifiState esp_wifi_state = S_WIFI_DISCONNECTED;
MQTTState esp_mqtt_state = S_MQTT_DISCONNECTED;
BLEState esp_ble_state = S_BLE_DISCONNECTED;

// Function to get current time in milliseconds
uint64_t millis() {
    return esp_timer_get_time() / 1000ULL;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static bool cmpColor(int arr1[3], int arr2[3]){
    if (arr1[0] == arr2[0] && arr1[1] == arr2[1] && arr1[2] == arr2[2]) {
        return true;
    }
    return false;
}


static led_strip_handle_t led_strip;

static void do_signal(int color[3])
{
    if(cmpColor(color, black)){
        led_strip_clear(led_strip);
        return;
    }
    //count LUMA
    if (luminance < 0) {
        luminance = 0;
    } else if (luminance > 255) {
        luminance = 255;
    }

    color[0] = (color[0] * luminance) / 255;
    color[1] = (color[1] * luminance) / 255;
    color[2] = (color[2] * luminance) / 255;
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_set_pixel(led_strip, 0, color[1], color[0], color[2]); //GBR
    /* Refresh the strip to send data */
    led_strip_refresh(led_strip);
}

static void do_signal_blink_panic(int color[3])
{
    int count = 5;
    int blink_len = 50;
    //
    for (int i = 0; i < count; ++i)
    {
        do_signal(color);
        vTaskDelay(blink_len / portTICK_PERIOD_MS);
        do_signal(black);
        vTaskDelay((CONFIG_INTERNAL_LED_PERIOD - (blink_len * count)) / count / portTICK_PERIOD_MS);
    }
}

static void do_signal_blink(int color[3])
{
    int count = 10;
    int blink_len = 50;
    //
    do_signal(color);
    vTaskDelay(blink_len / portTICK_PERIOD_MS);
    do_signal(black);
    vTaskDelay((CONFIG_INTERNAL_LED_PERIOD - (blink_len * count)) / count / portTICK_PERIOD_MS);
}

static void do_signal_no_period(int color[3])
{
    int blink_len = 50;
    //
    do_signal(color);
    vTaskDelay(blink_len / portTICK_PERIOD_MS);
    do_signal(black);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to do_signal addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_SHIELD_GPIO,
        .max_leds = LED_SHIELD_LENGHT, // at least one LED on board
    };
#if CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

/*
 * WIFI
 *
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_state = S_WIFI_CONNECTING;
        esp_wifi_connect();
        do_signal_no_period(pink);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            do_signal_no_period(pink);
            esp_wifi_state = S_WIFI_CONNECTING;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
            //nemozem posielat bit pretoze to ukonci proces wifi
        } else {
            esp_wifi_state = S_WIFI_DISCONNECTED;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        esp_wifi_state = S_WIFI_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


static void  wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID, .password = ESP_WIFI_PASS, /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT | WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_state = S_MQTT_CONNECTED;
        //esp_mqtt_client_enqueue is non-blocking
        //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html?highlight=mqtt
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        esp_mqtt_state = S_MQTT_DISCONNECTED;
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        //reconnection is fullz auto
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        esp_mqtt_state = S_MQTT_CONNECTED;
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        // TODO catch connecing error to set connecting state
        //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTING");
        //esp_wifi_state = S_MQTT_CONNECTING;
        
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = ESP_MQTT_HOST_URI,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    esp_mqtt_state = S_MQTT_CONNECTING;
}


static void ble_app_start(void){

}

void stopESP(){
    do_signal(deep);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_deep_sleep_start();
}

void loopWifi(){
    if(esp_wifi_state == S_WIFI_CONNECTED){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 30;
    ESP_LOGE(TAG, "[APP] loopWifi not esgablished ....");
    while (esp_wifi_state != S_WIFI_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(pink);
    }
}

void loopMQTT(){
    if(esp_mqtt_state == S_MQTT_CONNECTED){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 30;
    ESP_LOGE(TAG, "[APP] loopMQTT not esgablished ....");
    while (esp_mqtt_state != S_MQTT_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(yellow);
    }
}

void loopBLE(){
    if(esp_ble_state == S_BLE_CONNECTED){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 30;
    ESP_LOGE(TAG, "[APP] loopBLE not esgablished ....");
    while (esp_ble_state != S_BLE_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(blue);
    }
}



void app_main(void)
{

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    //ESP_ERROR_CHECK(esp_netif_init()); /// decalred in wifi init
    //ESP_ERROR_CHECK(esp_event_loop_create_default()); /// declared in wifi init

    /* Configure the peripheral according to the LED type */
    configure_led();
    do_signal(gray);


    wifi_init_sta();
    
    if (esp_wifi_state != S_WIFI_CONNECTED) {
        loopWifi();
        if (esp_wifi_state != S_WIFI_CONNECTED) {
            ESP_LOGE(TAG, "[APP] DIE DIE DIE ....");
            stopESP();
            return; //die
        }
    }

    mqtt_app_start();

    if (esp_mqtt_state != S_MQTT_CONNECTED) {
        loopMQTT();
        if (esp_mqtt_state != S_MQTT_CONNECTED) {
            ESP_LOGE(TAG, "[APP] DIE DIE DIE ....");
            stopESP();
            return; //die
        }
    }

    ble_app_start();

    if (esp_ble_state != S_BLE_CONNECTED) {
        loopBLE();
        if (esp_ble_state != S_BLE_CONNECTED) {
            //ESP_LOGE(TAG, "[APP] DIE DIE DIE ....");
            //stopESP(); //BLE does not break the bank
            //return; //die
        }
    }
  
    while (1) {
        do_signal(green);
        loopWifi();
        loopMQTT(); 
        loopBLE(); 
    }
}