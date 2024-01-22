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
#include "esp_mac.h"

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

// BLE https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/blecent/tutorial/blecent_walkthrough.md
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "gatt/blecent.h"

#include "driver/gpio.h"
#include "led_strip.h"

#define LOGGING 1

#define MQTT_DOWNSTREAM_TOPIC "/btmqtt/ccu/raw/downstream"
#define MQTT_UPSTREAM_TOPIC "/btmqtt/ccu/raw/upstream"

#define ESP_WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS       CONFIG_ESP_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY   CONFIG_ESP_MAXIMUM_RETRY
#define ESP_MQTT_HOST_URI   CONFIG_ESP_MQTT_HOST_URI

#define LED_SHIELD_GPIO     CONFIG_ESP_LED_PIN //CONFIG_LED_SHIELD_GPIO
#define LED_SHIELD_LENGHT   CONFIG_ESP_LED_PIN_LEN //CONFIG_LED_SHIELD_GPIO

#define CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_SPI true //force to spi //rmt is beter on 2 cores
#define CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_RMT false
#define CONFIG_INTERNAL_LED_PERIOD 1000

#define CUSTOM_PROTOCOL_TALLY_CATEGORY 128 //in CCU protocol this values are unused for now
#define CUSTOM_PROTOCOL_BLE_CATEGORY 129 //in CCU protocol this values are unused for now

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_FAIL_BIT       ( 1 << 0 ) //each BIT like 0000
#define WIFI_CONNECTED_BIT  ( 1 << 1 ) //each BIT like 0010
//this means that you set new bit and clear others...
// Define the enumeration
typedef enum {
    S_WIFI_DISCONNECTED,
    S_WIFI_CONNECTING,
    S_WIFI_CONNECTED,
    S_WIFI_TIMEOUT
    // Add more states as needed
} WifiState;

// Define the enumeration
typedef enum {
    S_MQTT_DISCONNECTED,
    S_MQTT_CONNECTING,
    S_MQTT_CONNECTED,
    S_MQTT_TIMEOUT
    // Add more states as needed
} MQTTState;

// Define the enumeration
typedef enum {
    S_BLE_DISCONNECTED,
    S_BLE_CONNECTING,
    S_BLE_CONNECTED,
    S_BLE_PASSCODE,
    S_BLE_RETRY,
    S_BLE_TIMEOUT
    // Add more states as needed
} BLEState;

// Define the enumeration
typedef enum {
    S_WHOIM_UNDEFINED,
    S_WHOIM_DEFINED,
    S_WHOIM_TIMEOUT
    // Add more states as needed
} WHOIMState;
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "ESP";

static int s_retry_num = 0;

uint8_t derived_mac_addr[6] = {0};

static led_strip_handle_t led_strip;

//signaling color
int luminance = 255;
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

int last_color[4] = {0, 0, 0, 0}; //last color stores luminance too

WifiState esp_wifi_state = S_WIFI_DISCONNECTED;
MQTTState esp_mqtt_state = S_MQTT_DISCONNECTED;
BLEState esp_ble_state = S_BLE_DISCONNECTED;
WHOIMState esp_whoim_state = S_WHOIM_UNDEFINED;

// this value have to be set from BLE handshaking
// this value determine which device this ESP represnets
// because CCU protocol use First Byte as "destination", so like CCU data for Camera 1 have first byte set to 01
// FF aka 255 is for broadcast for all devices
// we start as 255 to catch all messages, after MQTT handskage we will wait to "Who I M" message from controller
int who_im = 255; //255 means all data is for me
int signaling = 0;

esp_mqtt_client_handle_t mqtt_client;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

// Function to get current time in milliseconds
uint64_t millis() {
    return esp_timer_get_time() / 1000ULL;
}


void store_integer_value(const char* key, int value){
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, key, value)); //i32 means integer
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    // Close NVS handle
    nvs_close(nvs_handle);
}

int get_integer_value(const char* key) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));

    // Read the integer value with the provided key
    int32_t value;
    err = nvs_get_i32(nvs_handle, key, &value);
    if (err == ESP_OK) {
        //printf("Retrieved value from NVS with key '%s': %d\n", key, value);
    } else {
        //printf("Error getting value from NVS: %s\n", esp_err_to_name(err));
        // Return a default value or an error code
        value = -1;
    }

    // Close NVS handle
    nvs_close(nvs_handle);

    return value;
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

static void do_signal_color(int red, int green, int blue, int luma){
    if (luma < 0) {
        luma = 0;
    } else if (luma > 255) {
        luma = 255;
    }
    
    if(signaling == 0){
        //last color is set only for non-signaling signals
        last_color[0] = red;
        last_color[1] = green;
        last_color[2] = blue;
        last_color[3] = luma;
    }

    red = (red * luma) / 255;
    green = (green * luma) / 255;
    blue = (blue * luma) / 255;

    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_set_pixel(led_strip, 0, green, red, blue); //GBR
    /* Refresh the strip to send data */
    led_strip_refresh(led_strip);
}

static void do_signal_last(){
    do_signal_color(last_color[0],last_color[1],last_color[2],last_color[3]);
}

static void do_signal(int color[3])
{
    if(cmpColor(color, black)){
        led_strip_clear(led_strip);
        return;
    }
    //luminance is global
    return do_signal_color(color[0], color[1], color[2], luminance);
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


static bool check_signaling(){
    switch (signaling) {
        case 0:
            // return to last state
            do_signal_last();
            return false;
            break;
        case 1:
            do_signal(yellow);
            return true;
            break;
        case 2:
            //this is one time signal for operator
            do_signal_blink_panic(yellow);
            do_signal_blink_panic(yellow);
            do_signal_blink_panic(yellow);
            // return to last state
            do_signal_last();
            //reset signaling
            signaling = 0;
            return true;
            break;
        case 3:
            do_signal(blue);
            return true;
            break;
    }
    return false;
}

static void store_signal(int color[3]){
    last_color[0] = color[0];
    last_color[1] = color[1];
    last_color[2] = color[2];
    last_color[3] = luminance;
}
static void store_signal_color(int red, int green, int blue, int luma){
    last_color[0] = red;
    last_color[1] = green;
    last_color[2] = blue;
    last_color[3] = luma;
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

// Declare the function before using it
void mqttNotify(uint8_t id) {

    const uint8_t data[] = {0xFF, 0x05, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00, id};

    ESP_LOGW(TAG,"needPasskeyNofify");
    if(esp_mqtt_state != S_MQTT_CONNECTED){
        ESP_LOGE(TAG,"needPasskeyNofify MQTT NOT CONNECTED");
        return;
    }
    esp_mqtt_client_publish(mqtt_client, MQTT_DOWNSTREAM_TOPIC, (const char *)data, sizeof(data), 0, 0);
}

void sendBLE_Passkey(uint32_t passkey){
            // Prepare the structure with the passkey
        struct ble_sm_io io;
        io.action = BLE_SM_IOACT_INPUT;
        io.passkey = passkey;

        ble_sm_inject_io(conn_handle, &io);
    //esp_ble_passkey_reply(mqtt_client->ble_security.ble_req.bd_addr, true, passkey);
}

////////
// im trying to separate file for BT for better orientation
#include "gatt/gatt_client.c"
////////

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
        ESP_LOGE(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGW(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
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

// Function to convert raw data to a hexadecimal string with spaces and log using ESP_LOGI
void log_hex_data(const char *log_tag, const uint8_t *raw_data, size_t raw_data_len) {
    char hex_string[(raw_data_len * 3) + 1]; // Each byte takes 3 characters (2 hex + 1 space)
    size_t index = 0;

    for (size_t i = 0; i < raw_data_len; i++) {
        index += snprintf(&hex_string[index], sizeof(hex_string) - index, "%02X ", raw_data[i]);
    }

    ESP_LOGI(log_tag, "%s", hex_string);
}

uint32_t hexPayloadToUint32(const char *packet) {
    // Assuming payload is in little-endian order
    const uint8_t *payload = (const uint8_t*)(packet + 8); // Skip the first 8 bytes (headers)
    uint32_t result = 0;

    for (int i = 3; i >= 0; --i) {
        result = (result << 8) | payload[i];
    }

    return result;
}

static void onTallyOperation(esp_mqtt_event_handle_t event){
    //ESP_LOGW(TAG, "onTallyOperation: ");
    switch (event->data[5]) {
        case 0:
            //set signaling id
            signaling = event->data[8];
            //run signaling with new id
            check_signaling();
            break;
        case 1:
            if(event->data_len < 10){
                // the protocol at least 8 bytes
                ESP_LOGE(TAG, "Too few bytes for TALLY");
                return;
            }
            if(event->data[8] == who_im || event->data[8] == 255){
                //pgm
                if(check_signaling()){
                    ESP_LOGW(TAG, "OVERWITED BY SIGNALING > Store TALLY: PGM");
                }else{
                    ESP_LOGI(TAG, "Signal TALLY: PGM");
                    do_signal(red);
                } 
                store_signal(red);
            }else if(event->data[9] == who_im || event->data[9] == 255){
                //pvw
                if(check_signaling()){
                    ESP_LOGW(TAG, "OVERWITED BY SIGNALING > Store TALLY: PVW");
                }else{
                    ESP_LOGI(TAG, "Signal TALLY: PVW");
                    do_signal(green);
                }
                store_signal(green);
            }else{
                //off
                if(check_signaling()){
                    ESP_LOGW(TAG, "OVERWITED BY SIGNALING > Store TALLY: OFF");
                }else{
                    ESP_LOGI(TAG, "Signal TALLY: OFF");
                    do_signal(gray); //we signaling off as gray wo know that tally is working even is not in pgm/pvw state
                }   
                store_signal(gray);
            }
            break;
        case 2:
            if(event->data_len < 12){
                // the protocol at least 8 bytes
                ESP_LOGE(TAG, "Too few bytes for COLOR");
                return;
            }
            if(check_signaling()){
                ESP_LOGW(TAG, "OVERWITED BY SIGNALING > Signal COLOR");
            }else if(event->data[0] == who_im){
                //for color we check the destination
                ESP_LOGI(TAG, "Signal COLOR: ");
                // set tally by color / this ignore luminance setting
                //rgba
                do_signal_color(event->data[8], event->data[9], event->data[10], event->data[11]);
            }
            store_signal_color(event->data[8], event->data[9], event->data[10], event->data[11]);
            break;
    }
}

static void onBLEOperation(esp_mqtt_event_handle_t event){
    ESP_LOGW(TAG, "onBLEOperation: ");
    switch (event->data[5]) {
    case 0:
        //notify group
        //int8
        //Custom: 0 = disconnected, 1 = connecting, 2 = connected, 3 = need passcode, 4 = timeout
        //this is only ment for signaling from camera to the operator, from operator to the camera this have no reason
        break;
    case 1:
        //disconnect group
        //void
        //sendBLE_Disconnect();
        break;
    case 2:
        //connect group
        //uInt8
        if(event->data_len < 14){
            // the protocol at least 8 bytes
            ESP_LOGE(TAG, "Too few bytes for CONNECT");
            return;
        }
        int address[6] = {event->data[8],event->data[9],event->data[10],event->data[11],event->data[12],event->data[13]};
        //sendBLE_Connect(address);
        break;
    case 3:
        ESP_LOGW(TAG, "PASSSSSSSSSSSSS KEY");
        //passscode group
        uint32_t result = hexPayloadToUint32(event->data);

        ESP_LOGW(TAG, "Converted PASSSSSSSSSSSSS KEY value: %06u\n", (unsigned int)result);
        //{event->data[8],event->data[9],event->data[10],event->data[11],event->data[12],event->data[13]}
        sendBLE_Passkey(result);
        //sendBLE_Passcode(passcode);
        break;
    }
}

float convertFixed16ToFloat(uint8_t* packet) {
    // Extracting payload
    uint16_t payloadValue = (packet[9] << 8) | packet[8];

    // Sign extension for signed 16-bit integer
    int16_t signedValue = (int16_t)payloadValue;

    // Converting to float with scaling factor 2^11
    float floatValue = signedValue / 2048.0;
    ESP_LOGI(TAG, "Converted FLOAT Value (fixed16): %f\n", floatValue);
    return floatValue;
}

int convertFixed16ToInt(uint8_t* packet) {
    // Extract fixed16 value from the packet
    float floatValue = convertFixed16ToFloat(packet);

    // Convert the float value to an integer in the range [-16, 15]
    int intValue = (int)(floatValue * 255.0);
    ESP_LOGI(TAG, "Converted INT Value (fixed16): %d\n", intValue);
    return intValue;
}

static void onCCUOperation(esp_mqtt_event_handle_t event){
    ESP_LOGW(TAG, "onCCUOperation: ");


    //just forward data to the BLE
    //sendBLE(event->data);

    //catch CCU command for tally brightness and setup luminance
    if(event->data[4] == 5){
        ESP_LOGW(TAG, "CATCHING TALLY LUMA: ");
        switch (event->data[5]) {
            case 0:
            case 1:
            case 2:
                //all 0 = fron/rear 1 = front 2 = rear
                luminance = convertFixed16ToInt((uint8_t*)event->data); //data is fixed16
                if(luminance < 31){
                    luminance = 31;
                }
                if(luminance > 255){
                    luminance = 255;
                }
                store_integer_value("luminance", luminance);
                store_signal_color(last_color[0],last_color[1],last_color[2],luminance);
                do_signal_last();
                break;
            }     
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_state = S_MQTT_CONNECTED;
            //esp_mqtt_client_enqueue is non-blocking
            //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html?highlight=mqtt
            esp_mqtt_client_enqueue(client, "/system/heartbeat", (const char *)derived_mac_addr, sizeof(derived_mac_addr), 0, 0, true); //true means store for non-block
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            esp_mqtt_client_subscribe(client, MQTT_UPSTREAM_TOPIC, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            esp_mqtt_state = S_MQTT_DISCONNECTED;
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            //reconnection is fully auto
            //TOTO timeout???
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            esp_mqtt_state = S_MQTT_CONNECTED;
            ESP_LOGI(TAG, "MQTT_EVENT_DATA on LEN=%d TOPIC=%s\r\n", event->topic_len, event->topic);
            //because we are subscribed to the one topic, the topic is not relevant
            if(event->data_len < 8){
                // the protocol at least 8 bytes
                ESP_LOGE(TAG, "Too few bytes");
                return;
            }

            if(event->data[3] == who_im){
                // the byte 3 is reserved for nothing and should be 0, we use them for our purposes
                ESP_LOGW(TAG, "Loopback from myself");
                return;
            }

            if(event->data[0] == 255){
                //broadcast is for me
                event->data[0] = who_im;
            }
            //check data and determine the operation
            log_hex_data(TAG, (const uint8_t *)event->data, event->data_len);
            switch (event->data[4]) {
                case CUSTOM_PROTOCOL_TALLY_CATEGORY:
                    //tally
                    onTallyOperation(event);
                    break;
                case CUSTOM_PROTOCOL_BLE_CATEGORY:
                    //ble handshakes
                    onBLEOperation(event);
                    break;
                default:
                    //all others data is for CCU //todo - what data can broke the camera? Some valdation?
                    onCCUOperation(event);
                    break;
                }
            break;
        case MQTT_EVENT_ERROR:
            // TODO catch connecing error to set connecting state
            //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTING");
            //esp_wifi_state = S_MQTT_CONNECTING;
            
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
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

    //esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    esp_mqtt_state = S_MQTT_CONNECTING;
}


static void ble_app_start(void){
    ESP_LOGW(TAG, "[APP] Starting BLE sequence ....");
    BLEClient_app_main();
}

void stopESP(){
    do_signal(deep);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_deep_sleep_start();
}

void loopWifi(){
    if(esp_wifi_state == S_WIFI_CONNECTED || esp_wifi_state == S_WIFI_TIMEOUT){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 300;
    ESP_LOGE(TAG, "[APP] loopWifi not established ....");
    while (esp_wifi_state != S_WIFI_TIMEOUT && esp_wifi_state != S_WIFI_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(pink);
    }
    if(esp_wifi_state != S_WIFI_CONNECTED){
        esp_wifi_state = S_WIFI_TIMEOUT;
        do_signal(pink);
    }else{
        do_signal(gray);
    }
}

void loopMQTT(){
    if(esp_mqtt_state == S_MQTT_CONNECTED || esp_mqtt_state == S_MQTT_TIMEOUT){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 300;
    ESP_LOGE(TAG, "[APP] loopMQTT not established ....");
    while (esp_mqtt_state != S_MQTT_TIMEOUT && esp_mqtt_state != S_MQTT_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(orange);
    }
    if(esp_mqtt_state != S_MQTT_CONNECTED){
        esp_mqtt_state = S_MQTT_TIMEOUT;
        do_signal(orange);
    }else{
        do_signal(gray);
    }
}

void loopBLE(){
    if(esp_ble_state == S_BLE_CONNECTED || esp_ble_state == S_BLE_TIMEOUT){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 30;
    ESP_LOGE(TAG, "[APP] loopBLE not established ....");
    while (esp_ble_state != S_BLE_TIMEOUT && esp_ble_state != S_BLE_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(blue);
    }
    if(esp_ble_state != S_BLE_CONNECTED){
        esp_ble_state = S_BLE_TIMEOUT;
        do_signal(blue);
    }else{
        do_signal(gray);
    }
}

void loopWhoim(){
    if(who_im != 255 || esp_whoim_state == S_WHOIM_TIMEOUT){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 3;
    ESP_LOGE(TAG, "[APP] loopWhoim not established ....");
    while (esp_whoim_state != S_WHOIM_TIMEOUT && who_im == 255 && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(violet);
    }
    if(who_im == 255){
        esp_whoim_state = S_WHOIM_TIMEOUT;
        do_signal(violet);
    }else{
        do_signal(gray);
    }
}

void app_main(void)
{

    esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA);


    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "ESP32 ID: \n\n\n%02X:%02X:%02X:%02X:%02X:%02X\n\n\n",derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    if(LOGGING){
        esp_log_level_set("*", ESP_LOG_INFO);
        esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
        esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
        esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
        esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
        esp_log_level_set("transport", ESP_LOG_VERBOSE);
        esp_log_level_set("outbox", ESP_LOG_VERBOSE);
    }else{
        esp_log_level_set("*", ESP_LOG_NONE);
    }


    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    //ESP_ERROR_CHECK(esp_netif_init()); /// decalred in wifi init
    //ESP_ERROR_CHECK(esp_event_loop_create_default()); /// declared in wifi init

    /* Configure the peripheral according to the LED type */
    configure_led();
    store_signal(gray); //set default last color to gray
    do_signal_last();

    //boot up luminance is at full
    //after bootup load stored luminance
    const char* key = "luminance";
    luminance = get_integer_value(key);
    if(luminance < 0){
        luminance = 255;
    }

    
    wifi_init_sta();
    
    if (esp_wifi_state != S_WIFI_CONNECTED) {
        loopWifi();
        if (esp_wifi_state != S_WIFI_CONNECTED) {
            ESP_LOGE(TAG, "[APP] WIFI ... DIE DIE ....");
            stopESP();
            return; //die
        }
    }

    mqtt_app_start();

    if (esp_mqtt_state != S_MQTT_CONNECTED) {
        loopMQTT();
        if (esp_mqtt_state != S_MQTT_CONNECTED) {
            ESP_LOGE(TAG, "[APP] MQTT ... DIE DIE ....");
            stopESP();
            return; //die
        }
    }
    
    loopWhoim();

    ble_app_start();

    if (esp_ble_state != S_BLE_CONNECTED) {
        loopBLE();
        if (esp_ble_state != S_BLE_CONNECTED) {
            //ESP_LOGE(TAG, "[APP] BLE ... DIE DIE ....");
            //stopESP(); //BLE does not stop ESP, we still have Wifi and MQTT connection, we can use it as TALLY only.
            //return; //die
            //ble_stack_deinit();
        }
    }

    check_signaling();

    while (1) {
        loopWhoim();
        loopWifi();
        loopMQTT(); 
        //loopBLE(); 
        vTaskDelay(pdMS_TO_TICKS(1000)); //vTaskDelay(1000); //check every 1 second
    }
}