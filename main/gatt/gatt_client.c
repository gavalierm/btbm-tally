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

//static const ble_uuid_t * DeviceInformationService = BLE_UUID128_DECLARE(0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb );
//static const ble_uuid_t * DeviceInformationServiceV2 = BLE_UUID128_DECLARE(0x00, 0x00, 0x18, 0x0a, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb );

// this characteristic is interesting
// reverse
static const ble_uuid_t * CameraService_UUID = BLE_UUID128_DECLARE(0xD3,0x93,0xA8,0x0C,0xF3,0x86,0x77,0x8B,0xE6,0x11,0x75,0x6D,0x7A,0x56,0x1D,0x29);

static const ble_uuid_t * Timecode_UUID = BLE_UUID128_DECLARE(0xC8,0x76,0xE9,0x87,0x1D,0x45,0xFB,0x9A,0xBF,0x41,0xF1,0x86,0x10,0x21,0x8F,0x6D );
static const ble_uuid_t * OutgoingCameraControl_UUID = BLE_UUID128_DECLARE(0xbb,0xe1,0xf8,0xa2,0xec,0xd2,0x93,0x84,0x99,0x42,0xee,0x1a,0x5f,0x46,0xd3,0x5d);
static const ble_uuid_t * IncomingCameraControl_UUID = BLE_UUID128_DECLARE(0xd9,0x37,0x45,0x50,0x76,0x58,0x30,0xbf,0x6a,0x41,0xa0,0x76,0x40,0xe1,0x64,0xb8 );
static const ble_uuid_t * DeviceName_UUID = BLE_UUID128_DECLARE(0x9c,0xb8,0x2e,0x28,0x76,0xcc,0x63,0xb0,0xa0,0x41,0xfb,0xc9,0x52,0x0c,0xac,0xff );
static const ble_uuid_t * CameraStatus_UUID = BLE_UUID128_DECLARE(0xB9,0x51,0x9B,0x33,0x74,0xCA,0xBD,0x8A,0xC5,0x4F,0xDC,0x95,0x1D,0x69,0xE8,0x7F );
static const ble_uuid_t * ProtocolVersion_UUID = BLE_UUID128_DECLARE(0x06,0x27,0xEE,0x2B,0x39,0x3D,0x82,0x8F,0x6F,0x45,0x08,0xB5,0x18,0xD0,0x1F,0x8F);

static int blecent_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

ble_device_t discovered_devices[MAX_DISC_DEVICES + 1];
int discovered_devices_count = 0;

void ble_store_config_init(void);

/**
 * Logs information about a connection to the console.
 */
static void
blecent_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "handle=%d our_ota_addr_type=%d our_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->conn_handle, desc->our_ota_addr.type,
             desc->our_ota_addr.val[5],
             desc->our_ota_addr.val[4],
             desc->our_ota_addr.val[3],
             desc->our_ota_addr.val[2],
             desc->our_ota_addr.val[1],
             desc->our_ota_addr.val[0]);

    ESP_LOGI(TAG, "our_id_addr_type=%d our_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->our_id_addr.type,
             desc->our_id_addr.val[5],
             desc->our_id_addr.val[4],
             desc->our_id_addr.val[3],
             desc->our_id_addr.val[2],
             desc->our_id_addr.val[1],
             desc->our_id_addr.val[0]);

    ESP_LOGI(TAG, "peer_ota_addr_type=%d peer_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_ota_addr.type,
             desc->peer_ota_addr.val[5],
             desc->peer_ota_addr.val[4],
             desc->peer_ota_addr.val[3],
             desc->peer_ota_addr.val[2],
             desc->peer_ota_addr.val[1],
             desc->peer_ota_addr.val[0]);

    ESP_LOGI(TAG, "peer_id_addr_type=%d peer_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_id_addr.type,
             desc->peer_id_addr.val[5],
             desc->peer_id_addr.val[4],
             desc->peer_id_addr.val[3],
             desc->peer_id_addr.val[2],
             desc->peer_id_addr.val[1],
             desc->peer_id_addr.val[0]);

    ESP_LOGI(TAG, "conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

void print_discovered_devices() {
    ESP_LOGI("DEVICE_LIST", "Number of devices: %d", discovered_devices_count);
    for (int i = 0; i < MAX_DISC_DEVICES; i++) {
        if(discovered_devices[i].addr[0] == 0x00 && discovered_devices[i].addr[1] == 0x00 && discovered_devices[i].addr[2] == 0x00 && discovered_devices[i].addr[3] == 0x00 && discovered_devices[i].addr[4] == 0x00 && discovered_devices[i].addr[5] == 0x00){
            continue;
        }
        ESP_LOGI("DEVICE_LIST", "Device %d: %02X:%02X:%02X:%02X:%02X:%02X", i + 1, discovered_devices[i].addr[0], discovered_devices[i].addr[1], discovered_devices[i].addr[2], discovered_devices[i].addr[3], discovered_devices[i].addr[4], discovered_devices[i].addr[5]);
    }
}

static void blecent_log_mbuf(const struct os_mbuf *om){
        uint16_t data_len = OS_MBUF_PKTLEN(om);
        uint8_t *data = malloc(data_len);

        //print_mbuf(attr->om); MODLOG_DFLT(INFO, "END\n\n");

        if (data != NULL) {
            os_mbuf_copydata(om, 0, data_len, data);

            // Log the data using the ESP-IDF logging mechanism
            ESP_LOG_BUFFER_HEX("Notify Data", data, data_len);

            // Free the allocated buffer
            free(data);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for data");
        }
}

static int blecent_on_read(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    MODLOG_DFLT(INFO, "Read complete for the subscribable characteristic; status=%d conn_handle=%d", error->status, conn_handle);
    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        //print_mbuf(attr->om);
        blecent_log_mbuf(attr->om);
    }
    return 0;
}

static int blecent_read_log(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    if (error->status == 0) {
        blecent_log_mbuf(attr->om);
    }
    //free((void*)attr);
    return error->status; // non zero means die
}

static int after_read(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    MODLOG_DFLT(INFO, "Read complete for the subscribable characteristic; status=%d conn_handle=%d", error->status, conn_handle);
    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    return 0;
}

static int blecent_after_write(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    struct peer *peer;

    MODLOG_DFLT(INFO, "blecent_after_write complete; status=%d conn_handle=%d attr_handle=%d\n", error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    if (peer == NULL) {
        MODLOG_DFLT(ERROR, "Error in finding peer, aborting...");
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    const struct peer_chr *chr;
    int rc;
    chr = peer_chr_find_uuid(peer, CameraService_UUID, CameraStatus_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support CameraStatus_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE CameraStatus_UUID\n");
        rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_read_log, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; CameraStatus_UUID rc=%d", rc);
            goto err;
        }
    }
    return 0;
    /* Subscribe to, write to, and read the custom characteristic*/
    //blecent_custom_gatt_operations(peer);
err:
    return 0;

}

static int blecent_subscribe_to(const struct peer *peer, const ble_uuid_t *service_uuid_to_subscribe, bool notifications){

    MODLOG_DFLT(WARN, "START: blecent_subscribe_to\n");
    int rc;
    const struct peer_chr *chr;

    chr = peer_chr_find_uuid(peer, CameraService_UUID, service_uuid_to_subscribe);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support characteristic\n");
        goto err;
    }

    const struct peer_dsc *dsc;

    dsc = peer_dsc_find_uuid(peer, CameraService_UUID, service_uuid_to_subscribe, BLE_UUID16_DECLARE(0x2902)); // BLE_GATT_DSC_CLT_CFG_UUID16
    if (dsc != NULL) {
        MODLOG_DFLT(INFO, "INFO: Service HAVE DESCRIPTOR\n\n  conn_handle=%d\n  conn_handle=%d\n", peer->conn_handle, dsc->dsc.handle);
        //print_uuid(service_uuid_to_subscribe);
        uint8_t notifyOn[] = {0x01, 0x00};
        if(!notifications) notifyOn[0] = 0x02;

        /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/

        rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle, notifyOn, sizeof(notifyOn), NULL, NULL); //ble_gattc_write_no_rsp_flat nefunguje https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLERemoteCharacteristic.cpp
        if (rc != 0) {
            ESP_LOGW(TAG,"Service ERR %d\n",rc);
            return rc;
        }
    }else{
        MODLOG_DFLT(ERROR, "WARN: Peer doesn't support CCCD, its write only?\n");
    }
    return 0;
err:
    ESP_LOGE(TAG,"Terminate the connection.");
    return 1;
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

static void establishController(const struct peer *peer)
{
    //FF050000010A010002000000
    //BLE_GATT_CHR_F_NOTIFY


    ESP_LOGW(TAG, "\n\n\nestablishController\n\n\n");

    //uint8_t value[2];
    if (blecent_subscribe_to(peer, IncomingCameraControl_UUID, true) != 0) {
        MODLOG_DFLT(ERROR, "Error: IncomingCameraControl_UUID");
        //goto err;
    }else{
        MODLOG_DFLT(WARN,"OK: Subscribed; IncomingCameraControl_UUID");
    }

    if (blecent_subscribe_to(peer, Timecode_UUID, true) != 0) {
        MODLOG_DFLT(ERROR,"Error: Timecode_UUID");
        goto err;
    }else{
        MODLOG_DFLT(WARN,"OK: Subscribed; Timecode_UUID");
    }
    return;
    if (blecent_subscribe_to(peer, OutgoingCameraControl_UUID, true) != 0) {
        MODLOG_DFLT(ERROR, "Error: OutgoingCameraControl_UUID");
        //goto err;
    }else{
        MODLOG_DFLT(WARN,"OK: Subscribed; OutgoingCameraControl_UUID");
    }
        
    // rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_subscribe, (void*)Timecode_UUID);
    ESP_LOGI(TAG, "UUID MATCH UUID MATCH UUID MATCH UUID MATCH UUID MATCH ");

    const struct peer_chr *chr;
    chr = peer_chr_find_uuid(peer, CameraService_UUID, DeviceName_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't have DeviceName_UUID\n");
        goto err;
    }
    //vTaskDelay(1000);
    /* Write 1 byte to the new characteristic to test if it notifies after subscribing */
    const char *string_to_write = ble_svc_gap_device_name();
    uint16_t data_len = strlen(string_to_write);
    int rc;
    rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, string_to_write, data_len, blecent_after_write,  DeviceName_UUID); //, blecent_on_custom_write, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write to DeviceName_UUID; rc=%d\n", rc);
        goto err;
    }

    rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, string_to_write, data_len, blecent_after_write,  DeviceName_UUID); //, blecent_on_custom_write, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write to DeviceName_UUID; rc=%d\n", rc);
        goto err;
    }

    return;
err:
    ESP_LOGE(TAG,"Terminate the connection.");
    return;
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

static void blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        goto err;
    }

    MODLOG_DFLT(WARN, "Service discovery complete; status=%d conn_handle=%d\n", status, peer->conn_handle);

    // read unsecured first 
    const struct peer_chr *chr;
    int rc;

   chr = peer_chr_find_uuid(peer, CameraService_UUID, DeviceName_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support DeviceName_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE DeviceName_UUID\n");
        const char *myString = ble_svc_gap_device_name();
        size_t arraySize = strlen(myString);
        // Allocate an array of uint8_t to store the converted values
        uint8_t uint_name[arraySize];
        // Convert the string to uint8_t array
        stringToUint8Array(myString, uint_name, arraySize);

        ESP_LOGW(TAG, "Device name %s", uint_name);

        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, &uint_name, sizeof(uint_name), NULL, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to WRITE characteristic; DeviceName_UUID rc=%d", rc);
            goto err;
        }
    }
    // subscribe to the camera status
    //true means notifications with ACK aka INDICATIONS
    if (blecent_subscribe_to(peer, IncomingCameraControl_UUID, false) != 0) {
        MODLOG_DFLT(ERROR, "Error: IncomingCameraControl_UUID");
        //goto err;
    }else{
        MODLOG_DFLT(WARN,"OK: Subscribed; IncomingCameraControl_UUID");
    }
    // subscribe to the camera status
    //true means notifications without ACK
    if (blecent_subscribe_to(peer, Timecode_UUID, true) != 0) {
        MODLOG_DFLT(ERROR, "Error: IncomingCameraControl_UUID");
        //goto err;
    }else{
        MODLOG_DFLT(WARN,"OK: Subscribed; IncomingCameraControl_UUID");
    }
    /*
    // send 0x01 to the camera accroding to protocol
    chr = peer_chr_find_uuid(peer, CameraService_UUID, CameraStatus_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support CameraStatus_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE CameraStatus_UUID\n");
        int8_t value = {0x01};
        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, &value, sizeof(value), blecent_after_write, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to WRITE characteristic; CameraStatus_UUID rc=%d", rc);
            goto err;
        }
    }
    // send test data to the ccu
    vTaskDelay(5000);
    chr = peer_chr_find_uuid(peer, CameraService_UUID, OutgoingCameraControl_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support OutgoingCameraControl_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE OutgoingCameraControl_UUID\n");
        int8_t value[] = {0xFF,0x05,0x00,0x00,0x01,0x0A,0x01,0x00,0x02,0x00,0x00,0x00};
        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, value, sizeof(value), NULL, NULL); //NULL = send, wait for ACK, do nothing. no_rsp not working - write have to be with ACK
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to WRITE characteristic; OutgoingCameraControl_UUID rc=%d", rc);
            goto err;
        }
    }

    vTaskDelay(5000);
    // send test data to the ccu
    chr = peer_chr_find_uuid(peer, CameraService_UUID, OutgoingCameraControl_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support OutgoingCameraControl_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE OutgoingCameraControl_UUID\n");
        int8_t value[] = {0xFF,0x05,0x00,0x00,0x01,0x0A,0x01,0x00,0x00,0x00,0x00,0x00};
        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, value, sizeof(value), NULL, NULL); //NULL = send, wait for ACK, do nothing. no_rsp not working - write have to be with ACK
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to WRITE characteristic; OutgoingCameraControl_UUID rc=%d", rc);
            goto err;
        }
    }

    vTaskDelay(5000);
    chr = peer_chr_find_uuid(peer, CameraService_UUID, OutgoingCameraControl_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support OutgoingCameraControl_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE OutgoingCameraControl_UUID\n");
        int8_t value[] = {0xFF,0x05,0x00,0x00,0x01,0x0A,0x01,0x00,0x02,0x00,0x00,0x00};
        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, value, sizeof(value), NULL, NULL); //NULL = send, wait for ACK, do nothing. no_rsp not working - write have to be with ACK
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to WRITE characteristic; OutgoingCameraControl_UUID rc=%d", rc);
            goto err;
        }
    }

    vTaskDelay(5000);
    // send test data to the ccu
    chr = peer_chr_find_uuid(peer, CameraService_UUID, OutgoingCameraControl_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support OutgoingCameraControl_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE OutgoingCameraControl_UUID\n");
        int8_t value[] = {0xFF,0x05,0x00,0x00,0x01,0x0A,0x01,0x00,0x00,0x00,0x00,0x00};
        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, value, sizeof(value), NULL, NULL); //NULL = send, wait for ACK, do nothing. no_rsp not working - write have to be with ACK
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to WRITE characteristic; OutgoingCameraControl_UUID rc=%d", rc);
            goto err;
        }
    }

    return;

    chr = peer_chr_find_uuid(peer, BLE_UUID16_DECLARE(DeviceInformation_UUID), BLE_UUID16_DECLARE(CameraManufacturer_UUID));
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support CameraManufacturer_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE CameraManufacturer_UUID\n");
        rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_read_log, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; CameraManufacturer_UUID rc=%d", rc);
            goto err;
        }
    }
    chr = peer_chr_find_uuid(peer, CameraService_UUID, ProtocolVersion_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support ProtocolVersion_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE ProtocolVersion_UUID\n");
        rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_read_log, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; ProtocolVersion_UUID rc=%d", rc);
            goto err;
        }
    }
    chr = peer_chr_find_uuid(peer, BLE_UUID16_DECLARE(DeviceInformation_UUID), BLE_UUID16_DECLARE(CameraModel_UUID));
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support CameraModel_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "OK: Peer HAVE CameraModel_UUID\n");
        rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_read_log, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; CameraManufacturer_UUID rc=%d", rc);
            goto err;
        }
    }



    
    */

    //blecent_read_write_subscribe(peer);
    //establishController(peer);
    return;
err:
    /* Service discovery failed.  Terminate the connection. */
    MODLOG_DFLT(ERROR, "Error: blecent_on_disc_complete failed; status=%d conn_handle=%d\n", status, peer->conn_handle);
    //ble_gap_terminate(peer->conn_handle, BLE_ERR_UNSUPPORTED);
}



static int start_peer_discovery(struct ble_gap_event *event, void *arg){
        int rc;
        /*** Go for service discovery after encryption has been successfully enabled ***/
        rc = peer_disc_all(event->connect.conn_handle, blecent_on_disc_complete, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
            return rc;
        }
        return 0;
}





void print_ble_adv_fields(struct ble_hs_adv_fields *fields) {
    return;
    ESP_LOGI("BLE_ADV_FIELDS", "Parsed Advertisement Fields:");

    // Print flags
    ESP_LOGI("BLE_ADV_FIELDS", "Flags: 0x%02x", fields->flags);

    // Print complete name
    if (fields->name != NULL) {
        ESP_LOGI("BLE_ADV_FIELDS", "Name: %.*s", fields->name_len, fields->name);
    }

    // Print manufacturer-specific data
    if (fields->mfg_data != NULL) {
        ESP_LOGI("BLE_ADV_FIELDS", "Manufacturer Data: Len=%u", fields->mfg_data_len);
        
        // Convert mfg_data to a readable string
        char mfg_data_str[3 * BLE_HS_ADV_MAX_SZ + 1];  // Each byte takes 3 characters (2 hex digits + space)
        for (int i = 0; i < fields->mfg_data_len; i++) {
            sprintf(&mfg_data_str[3 * i], "%02X ", fields->mfg_data[i]);
        }
        ESP_LOGI("BLE_ADV_FIELDS", "Manufacturer Data: %s", mfg_data_str);
    }

    // Add more fields as needed based on your requirements

    ESP_LOGI("BLE_ADV_FIELDS", "End of Advertisement Fields");
}

static int check_interesting_devices(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;
    int i;

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        //ESP_LOGE(TAG,"NO FIELDS ON check_interesting_devices");
        return 0;
    }
    
    for (i = 0; i < fields.num_uuids16; i++) {
        ESP_LOGE(TAG,"FIELD > (0x%04X)", ble_uuid_u16(&fields.uuids128[i].u));
        if (ble_uuid_u16(&fields.uuids16[i].u) == Service_UUID) {
            ESP_LOGE(TAG,"Blackmagic Service_UUID (0x%04X)", ble_uuid_u16(&fields.uuids16[i].u));
            print_ble_adv_fields(&fields);
            return 1;
        }
    }
    return 0;
}

static void connect_becouse_interesting(ble_addr_t *addr){
    int rc;
    uint8_t own_addr_type;
    /* Scanning must be stopped before a connection. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
    //addr = &((struct ble_gap_disc_desc *)disc)->addr;

    rc = ble_gap_connect(own_addr_type, addr, 10000, NULL, blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d addr=%s; rc=%d\n", addr->type, addr_str(addr->val), rc);
        return;
    }
}











static int start_security(struct ble_gap_event *event, void *arg){
    ESP_LOGW(TAG,"Initiate security");
    /** Initiate security - It will perform
     * Pairing (Exchange keys)
     * Bonding (Store keys)
     * Encryption (Enable encryption)
     * Will invoke event BLE_GAP_EVENT_ENC_CHANGE
     **/
    int rc;
    rc = ble_gap_security_initiate(event->connect.conn_handle);
    if (rc != 0) {
        MODLOG_DFLT(INFO, "Security could not be initiated, rc = %d\n", rc);
        return ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    MODLOG_DFLT(INFO, "Connection secured\n");
    return 0;
}


static void notify_for_new_device(void *disc)
{
    // in this step we will check if PHPR device is interested with check_interesting_devices
    // if so we store ADDR into list and publish data to mqtt (addr and index)
    // TODO ? check device name and send name instead off address?
    ble_addr_t *addr;

    // the device is interesting
    addr = &((struct ble_gap_disc_desc *)disc)->addr;
    int8_t rssi = ((struct ble_gap_disc_desc *)disc)->rssi;
    ESP_LOGI(TAG,"Device found: %s, RSSI=%d", addr_str(addr->val), (int)rssi);
    // check if device is interesting
    // if not return (scanning is still active)
    if (!check_interesting_devices((struct ble_gap_disc_desc *)disc)) {
        return;
    }

    // the device is interesting
    discovered_devices_count++;
    if(discovered_devices_count > (MAX_DISC_DEVICES)){
        return;
    }

    ESP_LOGW(TAG,"Device is interesting: %s", addr_str(addr->val));
    memcpy(discovered_devices[discovered_devices_count - 1].addr, addr, ESP_BD_ADDR_LEN);
    // Extract and store the device name if available
    //esp_ble_gap_update_adv_data((struct ble_gap_disc_desc *)disc, ESP_BLE_AD_TYPE_NAME_CMPL, discovered_devices[discovered_devices_count].name);
    
    /// Here need to go MQTT message to populate discovered list
    //for testing we execute direct connect
    connect_becouse_interesting(addr);
    return;
}

static void blecent_scan(void)
{
    ESP_LOGW(TAG,"BLE START SCANNING");
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, 30000 , &disc_params, blecent_gap_event, NULL); //BLE_HS_FOREVER
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
}

static int blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    //ESP_LOGW(TAG, "blecent_gap_event");
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        ESP_LOGI(TAG, "blecent_gap_event BLE_GAP_EVENT_DISC");
        /* Try to connect to the advertiser if it looks interesting. */
        notify_for_new_device(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            
            //Remember peer. 
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }
            return start_security(event, arg);
        } else {
            /* Connection attempt failed; resume scanning. */
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n", event->connect.status);
            //blecent_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "blecent_gap_event BLE_GAP_EVENT_DISCONNECT");
        /* Connection terminated. */
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        blecent_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        ble_store_util_delete_oldest_peer();
        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        blecent_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "blecent_gap_event BLE_GAP_EVENT_DISC_COMPLETE");
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n", event->disc_complete.reason);

        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "blecent_gap_event BLE_GAP_EVENT_ENC_CHANGE encryption change event; status=%d", event->enc_change.status);
        
        //status have to be 0
        //non-zero means problem
        if(event->enc_change.status){
            ESP_LOGE(TAG, "\n\n\n\nDELETE BONDING\n\n\n\nBLE_GAP_EVENT_ENC_CHANGE status=%d", event->enc_change.status);
            /* Delete the old bond. */
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);
            /* Forget about peer. */
            peer_delete(event->enc_change.conn_handle);
            //terminate connection
            ble_gap_terminate(event->enc_change.conn_handle, event->enc_change.status);
            return 0;
        }
        struct ble_gap_conn_desc desc;
        int rc;
        /* Encryption has been enabled or disabled for this connection. */
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        MODLOG_DFLT(INFO, "Encryption has been enabled or disabled for this connection \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        blecent_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        return start_peer_discovery(event, arg);

    case BLE_GAP_EVENT_NOTIFY_RX:
        ESP_LOGI(TAG, "\nblecent_gap_event BLE_GAP_EVENT_NOTIFY_RX");
        /* Peer sent us a notification or indication. */
        MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d attr_len=%d", event->notify_rx.indication ? "indication" : "notification", event->notify_rx.conn_handle, event->notify_rx.attr_handle, OS_MBUF_PKTLEN(event->notify_rx.om));
        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        // Log the data from the notify_rx event
        //blecent_log_mbuf(event->notify_rx.om);
        MQTT_send(event->notify_rx.om);
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "blecent_gap_event BLE_GAP_EVENT_MTU");
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REATTEMPT_COUNT:
        ESP_LOGW("GAP_EVENT", "\n\n\n\n\n\nblecent_gap_event 29 TRY IT AGAIN %d\n\n\n\n\n\n",event->type);
        //i think from my experience that 29 means --- your last try ends with no reason.. if you really want connect do it again
        return BLE_GAP_EVENT_REATTEMPT_COUNT;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
    //case 29:
        
        ESP_LOGI("GAP_EVENT", "blecent_gap_event BLE_GAP_EVENT_REPEAT_PAIRING");
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI("GAP_EVENT", "BLE_GAP_EVENT_PASSKEY_ACTION Passkey action required!");
        // Request user input for the 6-digit passcode and convert it to integer
        mqttNotify(0x03); //global notification 0x03 means need pass

        conn_handle = event->passkey.conn_handle;
        return 0;
    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        ESP_LOGI("GAP_EVENT", "BLE_GAP_EVENT_IDENTITY_RESOLVED");
        return 0;    
    case BLE_GAP_EVENT_EXT_DISC:
        ESP_LOGI("GAP_EVENT", "BLE_GAP_EVENT_EXT_DISC");
        return 0;
    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        ESP_LOGI("GAP_EVENT", "blecent_gap_event BLE_GAP_EVENT_PHY_UPDATE_COMPLETE %d",event->type);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    default:
        ESP_LOGE("GAP_EVENT", "\n\n\n\n\n\n\n\n\n\n\n\nblecent_gap_event UNKNOWN %d\n\n\n\n\n\n\n\n\n\n\n\n",event->type);
        return 0;
    }
}

static void blecent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void blecent_on_sync(void)
{
    int rc;
    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    /* Begin scanning for a peripheral to connect to. */
    blecent_scan();
}

void blecent_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

static void ble_stack_deinit_init(void)
{
    // something like restart if needed
    int rc;

    ESP_LOGI(TAG, "Deinit host");

    rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    } else {
        ESP_LOGE(TAG, "Nimble port stop failed, rc = %d", rc);
    }

    vTaskDelay(1000); // because blecent_host_task will be executed

    ESP_LOGI(TAG, "Init host");

    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", rc);
        return;
    }

    nimble_port_freertos_init(blecent_host_task);

    ESP_LOGI(TAG, "BLE INIT DONE");
}

static void ble_stack_deinit(){
    int rc;

    ESP_LOGI(TAG, "Stop scanning before deinit");
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        //return;
    }

    vTaskDelay(1000); // because blecent_host_task will be executed

    ESP_LOGI(TAG, "Deinit host");

    rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    } else {
        ESP_LOGE(TAG, "Nimble port stop failed, rc = %d", rc);
    }

    print_discovered_devices();
}

void blecent_on_reg_callback(struct ble_gatt_register_ctxt * ctxt, void * clb){
    ESP_LOGW(TAG,"\n\n\n\n\n\n\n\n\n\n\nblecent_on_reg_callback\n\n\n\n\n\n\n\n\n\n\n");
}

void BLEClient_app_main(void)
{
    int rc;

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return;
    }

    /* Configure the host. */
    ble_hs_cfg.gatts_register_cb = blecent_on_reg_callback;
    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = BLE_HS_IO_KEYBOARD_ONLY; /// BLE_HS_IO_DISPLAY_YESNO BLE_HS_IO_KEYBOARD_ONLY
    ble_hs_cfg.sm_bonding = 1;
    //ble_hs_cfg.sm_oob_data_flag = 0;

    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;  
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    //ble_hs_cfg.sm_mitm = 1;
    //ble_hs_cfg.sm_sc = 1;
    //ble_hs_cfg.auth_req = BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM;

    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    rc = ble_svc_gap_device_name_set(esp_device_hostname);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(blecent_host_task);

    ESP_LOGI(TAG, "BLE INIT DONE");
}