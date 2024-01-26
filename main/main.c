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
#include "blecent.h"

#include "driver/gpio.h"
#include "led_strip.h"

#define LOGGING true

#define MQTT_DOWNSTREAM_TOPIC "/btmqtt/ccu/raw/downstream"
#define MQTT_UPSTREAM_TOPIC "/btmqtt/ccu/raw/upstream"

#define ESP_WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS       CONFIG_ESP_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY   CONFIG_ESP_MAXIMUM_RETRY
#define ESP_MQTT_HOST_URI   CONFIG_ESP_MQTT_HOST_URI

#define LED_SHIELD_GPIO     CONFIG_ESP_LED_PIN //CONFIG_LED_SHIELD_GPIO
#define LED_SHIELD_LENGHT   CONFIG_ESP_LED_PIN_LEN //CONFIG_LED_SHIELD_GPIO

#define CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_SPI false //force to spi 
#define CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_RMT true //rmt is beter on 2 cores?
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
    STATE_DISCONNECTED,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_TIMEOUT,
    STATE_CAM_ID,
    STATE_PASSCODE
} ESPState;

/* FreeRTOS event group to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;

static const char *APP_TAG = "[ APP ] ";
static const char *BLE_TAG = "[ BLE ] ";
static const char *MQTT_TAG = "[ MQTT ] ";
static const char *WHOIM_TAG = "[ WHOIM ] ";
static const char *WIFI_TAG = "[ WIFI ] ";
static const char *LED_TAG = "[ LED ] ";

#define CONFIG_EXAMPLE_EXTENDED_ADV 0
#define MAX_DISC_DEVICES 10

#ifndef ESP_BD_ADDR_LEN
#define ESP_BD_ADDR_LEN 6
#endif
#ifndef ESP_BLE_ADV_DATA_LEN_MAX
#define ESP_BLE_ADV_DATA_LEN_MAX 20 // Maximum length of the device name
#endif

typedef struct {
    uint8_t addr[ESP_BD_ADDR_LEN];
    char name[ESP_BLE_ADV_DATA_LEN_MAX]; // Assume maximum length for simplicity
} ble_device_t;

#define Service_UUID 0x1800

#define DeviceInformation_UUID 0x180A
#define CameraManufacturer_UUID 0x2A29
#define CameraModel_UUID 0x2A24

static const ble_uuid_t * CameraService_UUID = BLE_UUID128_DECLARE(0xD3,0x93,0xA8,0x0C,0xF3,0x86,0x77,0x8B,0xE6,0x11,0x75,0x6D,0x7A,0x56,0x1D,0x29);

static const ble_uuid_t * Timecode_UUID = BLE_UUID128_DECLARE(0xC8,0x76,0xE9,0x87,0x1D,0x45,0xFB,0x9A,0xBF,0x41,0xF1,0x86,0x10,0x21,0x8F,0x6D );
static const ble_uuid_t * OutgoingCameraControl_UUID = BLE_UUID128_DECLARE(0xbb,0xe1,0xf8,0xa2,0xec,0xd2,0x93,0x84,0x99,0x42,0xee,0x1a,0x5f,0x46,0xd3,0x5d);
static const ble_uuid_t * IncomingCameraControl_UUID = BLE_UUID128_DECLARE(0xd9,0x37,0x45,0x50,0x76,0x58,0x30,0xbf,0x6a,0x41,0xa0,0x76,0x40,0xe1,0x64,0xb8 );
static const ble_uuid_t * DeviceName_UUID = BLE_UUID128_DECLARE(0x9c,0xb8,0x2e,0x28,0x76,0xcc,0x63,0xb0,0xa0,0x41,0xfb,0xc9,0x52,0x0c,0xac,0xff );
static const ble_uuid_t * CameraStatus_UUID = BLE_UUID128_DECLARE(0xB9,0x51,0x9B,0x33,0x74,0xCA,0xBD,0x8A,0xC5,0x4F,0xDC,0x95,0x1D,0x69,0xE8,0x7F );
static const ble_uuid_t * ProtocolVersion_UUID = BLE_UUID128_DECLARE(0x06,0x27,0xEE,0x2B,0x39,0x3D,0x82,0x8F,0x6F,0x45,0x08,0xB5,0x18,0xD0,0x1F,0x8F);


static int s_retry_num = 0;

// this value have to be set from BLE handshaking
// this value determine which device this ESP represnets
// because CCU protocol use First Byte as "destination", so like CCU data for Camera 1 have first byte set to 01
// FF aka 255 is for broadcast for all devices
// we start as 255 to catch all messages, after MQTT handskage we will wait to "Who I M" message from controller
int who_im = 99; //00 means all data is for me
int signaling = 0;
//
uint8_t esp_mac_address[6] = {0};
char esp_device_hostname[] = "ESP-BLE-XXYYZZ-00";

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

ESPState esp_wifi_state = STATE_DISCONNECTED;
ESPState esp_mqtt_state = STATE_DISCONNECTED;
ESPState esp_ble_state = STATE_DISCONNECTED;
ESPState esp_whoim_state = STATE_DISCONNECTED;

esp_mqtt_client_handle_t mqtt_client;
static uint16_t ble_connection_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t ble_write_handle = BLE_HS_CONN_HANDLE_NONE;


#include "utils.c"
#include "rgb_tally_functions.c"

void BLE_notifyMqtt(uint8_t id) {
    if(esp_mqtt_state != STATE_CONNECTED){
        ESP_LOGE(BLE_TAG,"BLE_notifyMqtt MQTT NOT CONNECTED");
        return;
    }
    //BLE is custom notification and is defined in PROTOCOL
    //struct is consitent we change only last byte
    //Custom: 0 = disconnected, 1 = connecting, 2 = connected, 3 = timeout, 4 = need passcode, 5 = need camera_id
    const uint8_t data[] = {0xFF, 0x05, 0x00, 0x00, 0x81, 0x00, 0x01, 0x00, id, 0x00, 0x00, 0x00};

    //ESP_LOGW(BLE_TAG,"BLE_notifyMqtt");

    esp_mqtt_client_publish(mqtt_client, MQTT_DOWNSTREAM_TOPIC, (const char *)data, 12, 0, 0);
}

int hearbbeat_counter = 0;
void BLE_onReceive(const struct os_mbuf *om){
    if(esp_mqtt_state != STATE_CONNECTED){
        return;
    }
    uint16_t data_len = OS_MBUF_PKTLEN(om);
    uint8_t *data = malloc(data_len);
    
    if (data != NULL) {
        // copy data as is recomended in CENT example
        os_mbuf_copydata(om, 0, data_len, data);
        //
        if(data[4] == 0x09 && data[5] == 0x00){ // 9 means operation type timecode 0 means thick
            //ESP_LOGW(BLE_TAG,"DATA Timecode Thick");
            if (hearbbeat_counter >= 15) {
                hearbbeat_counter = 0;
                esp_mqtt_client_enqueue(mqtt_client, "/system/heartbeat", (const char *)esp_device_hostname, sizeof(esp_device_hostname), 0, 0, true); //true means store for non-block
            }
            hearbbeat_counter++;
            // Free the allocated buffer
            free(data);
            return;
        }

        if(data[4] == 0x0c && data[5] == 0x05){
            // META tag CAMERA is on 0c 05 
            // ff 05 00 00 0c 05 05 02 38 ...
            size_t len = data[1];  
            int camera_id = 0;
            int firstDigit = 0;
            int secondDigit = 0;
            ESP_LOGW(APP_TAG,"META Camera ID was changed %zu / %zu", len, data_len);
            if (len > 1) {
                if(data[8] >= '0' && data[8] <= '9' && data[9] >= '0' && data[9] <= '9'){
                    //two digid
                    firstDigit = data[8] - '0';
                    secondDigit = data[9] - '0';
                }else if(data[8] >= '0' && data[8] <= '9'){
                    firstDigit = 0;
                    secondDigit = data[8] - '0';
                }           
            }else{
                if(data[8] >= '0' && data[8] <= '9'){
                    firstDigit = 0;
                    secondDigit = data[8] - '0';
                }
            }
            // Combine the digits to form a two-digit number
            if((firstDigit * 10 + secondDigit) > 0 && (firstDigit * 10 + secondDigit) < 99){
                who_im = firstDigit * 10 + secondDigit;
            }else{
                who_im = 99;
            }
            update_esp_name();
        }
        //return;
        ESP_LOG_BUFFER_HEX("DATA", data, data_len); //Log is formated to 16 bytes per LINE
        // send data to mqtt using enqueue whis is async-like behavior
        // sending from camera to the operator do not have priorty
        esp_mqtt_client_enqueue(mqtt_client, MQTT_DOWNSTREAM_TOPIC, (const char *)data, data_len, 0, 0, true);

        // Free the allocated buffer
        free(data);
    } else {
        ESP_LOGE(BLE_TAG, "Failed to allocate memory for data");
    }
}

////////
#include "gatt/gatt_client.c"
////////

void BLE_sendPasskey(uint32_t passkey){
    if(esp_ble_state != STATE_CONNECTING && esp_ble_state != STATE_PASSCODE){
        ESP_LOGE(APP_TAG,"Passkey STATE_CONNECTING ? rc=%d", esp_ble_state);
        return;
    }
    // Prepare the structure with the passkey
    struct ble_sm_io io;
    io.action = BLE_SM_IOACT_INPUT;
    io.passkey = passkey;
    ble_sm_inject_io(ble_connection_handle, &io);
}

void BLE_sendUnsubscribe(){
    if(esp_ble_state != STATE_CONNECTED){
        return;
    }
    blecent_unsubscribe_subscribe_incoming();
}
void BLE_sendClearBond(){
    if(esp_ble_state != STATE_CONNECTED && esp_ble_state != STATE_CONNECTING){
        return;
    }
    ble_store_util_delete_oldest_peer();
}
void BLE_sendConnect(uint8_t *addr){
    if(esp_ble_state == STATE_CONNECTING || esp_ble_state == STATE_CONNECTED || esp_ble_state == STATE_PASSCODE){
        return;
    }
    connect_to_addr(addr);
}

void BLE_sendDisconnect(){
    if(esp_ble_state == STATE_DISCONNECTED){
        return;
    }
    ble_gap_terminate(ble_connection_handle, BLE_ERR_REM_USER_CONN_TERM);
}
void BLE_sendData(const uint8_t *event_data, size_t data_len){
    if(esp_ble_state != STATE_CONNECTED){
        ESP_LOGW(APP_TAG,"BLE_sendData not connected");
        return;
    }
    // Convert data
    //uint8_t* nimbleData = convertData(event_data, data_len);

    // Use ble_gattc_write_flat with nimbleData
    //ble_gattc_write_flat(ble_connection_handle, ble_write_handle, nimbleData, event_data_len * sizeof(uint32_t), NULL, NULL);
    //ESP_LOGW(APP_TAG,"\n\n\n\nBLE_sendData Send....\n\n\n\n");
    blecent_write_to_outgoing(event_data, data_len);
}
/*
 * WIFI
 *
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_state = STATE_CONNECTING;
        //do_signal_no_period(pink);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            //do_signal_no_period(pink);
            esp_wifi_state = STATE_CONNECTING;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(APP_TAG, "retry to connect to the AP");
            //nemozem posielat bit pretoze to ukonci proces wifi
        } else {
            esp_wifi_state = STATE_DISCONNECTED;
            //xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(WIFI_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGW(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        esp_wifi_state = STATE_CONNECTED;
        //xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


static void  wifi_app_start(void)
{
    esp_wifi_state = STATE_CONNECTING;
    //s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // set hostname
    esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), esp_device_hostname);

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

    ESP_LOGI(WIFI_TAG, "wifi_app_start finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
    //EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT | WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. 
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }

    // The event will not be processed after unregister
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
    */
}

static void MQTT_onReceiveTally(esp_mqtt_event_handle_t event){
    //FF 05 00 00 81 03 00 00 505b06 00 //pass
    //
    //ESP_LOGW(APP_TAG, "MQTT_onReceiveTally: ");
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
                ESP_LOGE(APP_TAG, "Too few bytes for TALLY");
                return;
            }
            if(event->data[8] == who_im || event->data[8] == 255){
                //pgm
                if(check_signaling()){
                    ESP_LOGW(APP_TAG, "OVERWITED BY SIGNALING > Store TALLY: PGM");
                }else{
                    //ESP_LOGI(APP_TAG, "Signal TALLY: PGM");
                    do_signal(red);
                } 
                store_signal(red);
            }else if(event->data[9] == who_im || event->data[9] == 255){
                //pvw
                if(check_signaling()){
                    ESP_LOGW(APP_TAG, "OVERWITED BY SIGNALING > Store TALLY: PVW");
                }else{
                    //ESP_LOGI(APP_TAG, "Signal TALLY: PVW");
                    do_signal(green);
                }
                store_signal(green);
            }else{
                //off
                if(check_signaling()){
                    ESP_LOGW(APP_TAG, "OVERWITED BY SIGNALING > Store TALLY: OFF");
                }else{
                    //ESP_LOGI(APP_TAG, "Signal TALLY: OFF");
                    do_signal(gray); //we signaling off as gray wo know that tally is working even is not in pgm/pvw state
                }   
                store_signal(gray);
            }
            break;
        case 2:
            if(event->data_len < 12){
                // the protocol at least 8 bytes
                ESP_LOGE(APP_TAG, "Too few bytes for COLOR");
                return;
            }
            if(check_signaling()){
                ESP_LOGW(APP_TAG, "OVERWITED BY SIGNALING > Signal COLOR");
            }else if(event->data[0] == who_im){
                //for color we check the destination
                //ESP_LOGI(APP_TAG, "Signal COLOR: ");
                // set tally by color / this ignore luminance setting
                //rgba
                do_signal_color(event->data[8], event->data[9], event->data[10], event->data[11]);
            }
            store_signal_color(event->data[8], event->data[9], event->data[10], event->data[11]);
            break;
    }
}

static void MQTT_onReceiveBLE(esp_mqtt_event_handle_t event){
    ESP_LOGW(APP_TAG, "MQTT_onReceiveBLE: ");
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
        BLE_sendDisconnect();
        break;
    case 2:
        //connect group
        //uInt8
        if(event->data_len < 14){
            // the protocol at least 8 bytes
            ESP_LOGE(APP_TAG, "Too few bytes for CONNECT");
            return;
        }
        int address[6] = {event->data[8],event->data[9],event->data[10],event->data[11],event->data[12],event->data[13]};
        //sendBLE_Connect(address);
        break;
    case 3:
        ESP_LOGW(APP_TAG, "PASSSSSSSSSSSSS KEY");
        //passscode group
        uint32_t result = hexPayloadToUint32(event->data);

        ESP_LOGW(APP_TAG, "Converted PASSSSSSSSSSSSS KEY value: %06u\n", (unsigned int)result);
        //{event->data[8],event->data[9],event->data[10],event->data[11],event->data[12],event->data[13]}
        BLE_sendPasskey(result);
        //sendBLE_Passcode(passcode);
        break;
    }
}

static void MQTT_onReceiveCCU(esp_mqtt_event_handle_t event){
    ESP_LOGW(APP_TAG, "MQTT_onReceiveCCU: ");


    //just forward data to the BLE
    //sendBLE(event->data);

    //catch CCU command for tally brightness and setup luminance
    if(event->data[4] == 5){
        ESP_LOGW(APP_TAG, "CATCHING TALLY LUMA: ");
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

    ESP_LOGW(APP_TAG, "MQTT_onReceiveCCU: BLE_sendData %d, %s", event->data_len, event->data);
    //
    // All data sent to the camera have to have 0xFF as destination, otherwise will be rejected
    // because we use first byte as destination to determine camera id we need this to overwrite here
    event->data[0] = 0xff;
    //
    BLE_sendData((const uint8_t *)event->data, event->data_len);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_state = STATE_CONNECTED;
            //esp_mqtt_client_enqueue is non-blocking
            //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html?highlight=mqtt
            esp_mqtt_client_enqueue(client, "/system/heartbeat", (const char *)esp_device_hostname, sizeof(esp_device_hostname), 0, 0, true); //true means store for non-block
            //ESP_LOGI(APP_TAG, "sent publish successful, msg_id=%d", msg_id);
            esp_mqtt_client_subscribe(client, MQTT_UPSTREAM_TOPIC, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            esp_mqtt_state = STATE_DISCONNECTED;
            ESP_LOGI(APP_TAG, "MQTT_EVENT_DISCONNECTED");
            //reconnection is fully auto
            //TOTO timeout???
            break;

        case MQTT_EVENT_SUBSCRIBED:
            //ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            //ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            //ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            esp_mqtt_state = STATE_CONNECTED;
            //ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA on LEN=%d TOPIC=%s\r\n", event->topic_len, event->topic);
            //because we are subscribed to the one topic, the topic is not relevant
            if(event->data_len < 8){
                // the protocol at least 8 bytes
                ESP_LOGE(MQTT_TAG, "Too few bytes");
                return;
            }

            if(event->data[3] == who_im){
                // the byte 3 is reserved for nothing and should be 0, we use them for our purposes
                ESP_LOGW(MQTT_TAG, "Loopback from myself");
                return;
            }

            if(event->data[0] == 255){
                //broadcast is for me
                event->data[0] = who_im;
            }
            //check data and determine the operation
            log_hex_data(MQTT_TAG, (const uint8_t *)event->data, event->data_len);
            switch (event->data[4]) {
                case CUSTOM_PROTOCOL_TALLY_CATEGORY:
                    //tally
                    MQTT_onReceiveTally(event);
                    break;
                case CUSTOM_PROTOCOL_BLE_CATEGORY:
                    //ble handshakes
                    MQTT_onReceiveBLE(event);
                    break;
                default:
                    //all others data is for CCU //todo - what data can broke the camera? Some valdation?
                    MQTT_onReceiveCCU(event);
                    break;
                }
            break;
        case MQTT_EVENT_ERROR:
            // TODO catch some error to set connecting state
            //ESP_LOGI(APP_TAG, "MQTT_EVENT_CONNECTING");
            //esp_wifi_state = STATE_CONNECTING;
            
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(APP_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_state = STATE_CONNECTING;
    //
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = ESP_MQTT_HOST_URI,
    };

    //esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}


static void ble_app_start(void){
    ESP_LOGW(APP_TAG, "Starting BLE sequence ....");
    BLEClient_app_main();
}

void loopWifi(){
    if(esp_wifi_state == STATE_CONNECTED || esp_wifi_state == STATE_TIMEOUT){
        return;
    }
    uint64_t startTime = millis();
    int timeout = 300;
    ESP_LOGE(APP_TAG, "loopWifi not established ....");
    while (esp_wifi_state != STATE_TIMEOUT && esp_wifi_state != STATE_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(red);
    }
    if(esp_wifi_state != STATE_CONNECTED){
        esp_wifi_state = STATE_TIMEOUT;
        do_signal(red);
    }else{
        do_signal(gray);
    }
}

void loopMQTT(){
    if(esp_mqtt_state == STATE_CONNECTED || esp_mqtt_state == STATE_TIMEOUT){
        vTaskDelay(10);
        return;
    }
    uint64_t startTime = millis();
    int timeout = 300;
    ESP_LOGE(APP_TAG, "loopMQTT not established ....");
    while (esp_mqtt_state != STATE_TIMEOUT && esp_mqtt_state != STATE_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(orange);
    }
    if(esp_mqtt_state != STATE_CONNECTED){
        esp_mqtt_state = STATE_TIMEOUT;
        do_signal(orange);
    }else{
        do_signal(gray);
    }
}

void loopBLE(){
    if(esp_ble_state == STATE_CONNECTED || esp_ble_state == STATE_TIMEOUT){
        vTaskDelay(10);
        return;
    }
    uint64_t startTime = millis();
    int timeout = 300;
    ESP_LOGE(APP_TAG, "loopBLE not established ....");
    while (esp_ble_state != STATE_TIMEOUT && esp_ble_state != STATE_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(blue);
    }
    if(esp_ble_state != STATE_CONNECTED){
        esp_ble_state = STATE_TIMEOUT;
        do_signal(blue);
        //terminate the BLE?
    }else{
        do_signal(gray);
    }
}

void loopWhoim(){
    if(who_im != 99 || who_im != 0 || esp_whoim_state == STATE_CONNECTED){ //whoim without timeout!!! whoim is essential
        vTaskDelay(10);
        return;
    }
    uint64_t startTime = millis();
    int timeout = 3;
    ESP_LOGE(APP_TAG, "loopWhoim not established ....");
    //setup notification data
    //const uint8_t data[] = {0xFF, 0x05, 0x00, 0x00, 0x81, 0x01, 0x01, 0x00, 0x00};

    while ( who_im >= 99 && who_im <= 0 && esp_whoim_state != STATE_TIMEOUT && esp_whoim_state != STATE_CONNECTED && ( (millis() - startTime) < (timeout * 1000) ) ) {
        do_signal_blink_panic(violet);
    }
    if(who_im >= 99 || who_im <= 0){
        esp_whoim_state = STATE_TIMEOUT;
        do_signal(violet);
    }else{
        do_signal(gray);
    }
}

void app_main(void)
{
    ESP_LOGI(APP_TAG, "\n\n\n\n\nStartup...\n\n\n\n\n");

    //populate the esp_mac_address
    esp_read_mac(esp_mac_address, ESP_MAC_WIFI_STA);
    ESP_LOGW(APP_TAG, "ESP32 MAC: \n%02X:%02X:%02X:%02X:%02X:%02X\n\n\n",esp_mac_address[0], esp_mac_address[1], esp_mac_address[2],esp_mac_address[3], esp_mac_address[4], esp_mac_address[5]);

    ESP_LOGI(APP_TAG, "Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(APP_TAG, "IDF version: %s", esp_get_idf_version());

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

    // Configure the peripheral according to the LED type
    configure_led();
    store_signal(gray); //set default last color to gray
    do_signal_last(); //boot up with gray status to indicate uptime

    //
    who_im = get_integer_value("who_im");
    if(who_im <= 0 || who_im >= 99){
        who_im = 99;
    }
    
    //
    update_esp_name();
    ESP_LOGW(APP_TAG, "\n\n\nESP Hostname: %s\n\n\n", esp_device_hostname);
    //

    // start the app
    wifi_app_start(); 
    loopWifi();
    if (esp_wifi_state != STATE_CONNECTED) {
        ESP_LOGE(APP_TAG, "WIFI ... DIE ...");
        return; //die
    }
    //
    mqtt_app_start();
    loopMQTT();
    if (esp_mqtt_state != STATE_CONNECTED) {
        //the time for WHOIM is 15 minutes, then die
        ESP_LOGE(APP_TAG, "MQTT ... DIE ...");
        return; //die
    }
    //
    ble_app_start();
    loopBLE();
    if (esp_ble_state != STATE_CONNECTED) {
        //BLE is not connected intime so we deinit the stack to save some CPU/RAM
        //ble_stack_deinit();
        //return; //die
    }
    // there is a option to set who_im - via "Camera ID" in Project properties on BM camera
    // first two chars have to be int like "1" and "2" means 12
    // because the target for this project is BM camera we try to obtain the ID from BLE
    // if who_im is obtained from Camera loopWhoim skips
    // if not there is no reason to continue because ESP do not know what Camera ID needs to follow
    loopWhoim();
    if (esp_whoim_state != STATE_CONNECTED) {
        ESP_LOGE(APP_TAG, "WHOIM ... DIE ...");
        //Whoim is essential we will wait until whoim will be set
        while(esp_whoim_state != STATE_CONNECTED){
            loopWhoim();
            vTaskDelay(5000);
        }
    }
    ESP_LOGE(APP_TAG, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nWHOIM\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    //boot up luminance is at full
    //after bootup load stored luminance
    luminance = get_integer_value("luminance");
    if(luminance < 0){
        luminance = 255;
    }
    check_signaling();
    bool test = false;
    bool on = 0; //for testing purpose
    uint8_t data_array[12]; //for testing purpose

    while (1) {
        // This is only for testing purpose
        
        if(esp_ble_state == STATE_CONNECTED && test == true){
            if (on) {
                // Update values if 'on' is true
                memcpy(data_array, (uint8_t[]){0xFF, 0x05, 0x00, 0x00, 0x01, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, sizeof(data_array));
            } else {
                // Update values if 'on' is false
                memcpy(data_array, (uint8_t[]){0xFF, 0x05, 0x00, 0x00, 0x01, 0x0A, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00}, sizeof(data_array));
            }
            on = !on;
            // Call the function with the data array
            BLE_sendData(data_array, 12);    
        }
        

        // essential to keep status of all services
        loopWifi();
        loopMQTT(); 
        loopBLE(); 
        loopWhoim();
        vTaskDelay(pdMS_TO_TICKS(5000)); // every 5 seconds
    }
}