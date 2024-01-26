static int blecent_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

uint8_t discovered_devices[MAX_DISC_DEVICES + 1];
int discovered_devices_count = 0;

void ble_store_config_init(void);

static const char* uuid_str(const ble_uuid_t *uuid) {
    if (uuid == CameraService_UUID) {
        return "CameraService_UUID";
    } else if (uuid == Timecode_UUID) {
        return "Timecode_UUID";
    } else if (uuid == OutgoingCameraControl_UUID) {
        return "OutgoingCameraControl_UUID";
    } else if (uuid == IncomingCameraControl_UUID) {
        return "IncomingCameraControl_UUID";
    } else if (uuid == DeviceName_UUID) {
        return "DeviceName_UUID";
    } else if (uuid == CameraStatus_UUID) {
        return "CameraStatus_UUID";
    } else if (uuid == ProtocolVersion_UUID) {
        return "ProtocolVersion_UUID";
    }
    return "Unknown UUID";
}

/**
 * Logs information about a connection to the console.
 */
static void
blecent_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(BLE_TAG, "handle = %d our_ota_addr_type = %d our_ota_addr = %02x:%02x:%02x:%02x:%02x:%02x",
             desc->conn_handle, desc->our_ota_addr.type,
             desc->our_ota_addr.val[5],
             desc->our_ota_addr.val[4],
             desc->our_ota_addr.val[3],
             desc->our_ota_addr.val[2],
             desc->our_ota_addr.val[1],
             desc->our_ota_addr.val[0]);

    ESP_LOGI(BLE_TAG, "our_id_addr_type = %d our_id_addr = %02x:%02x:%02x:%02x:%02x:%02x",
             desc->our_id_addr.type,
             desc->our_id_addr.val[5],
             desc->our_id_addr.val[4],
             desc->our_id_addr.val[3],
             desc->our_id_addr.val[2],
             desc->our_id_addr.val[1],
             desc->our_id_addr.val[0]);

    ESP_LOGI(BLE_TAG, "peer_ota_addr_type = %d peer_ota_addr = %02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_ota_addr.type,
             desc->peer_ota_addr.val[5],
             desc->peer_ota_addr.val[4],
             desc->peer_ota_addr.val[3],
             desc->peer_ota_addr.val[2],
             desc->peer_ota_addr.val[1],
             desc->peer_ota_addr.val[0]);

    ESP_LOGI(BLE_TAG, "peer_id_addr_type = %d peer_id_addr = %02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_id_addr.type,
             desc->peer_id_addr.val[5],
             desc->peer_id_addr.val[4],
             desc->peer_id_addr.val[3],
             desc->peer_id_addr.val[2],
             desc->peer_id_addr.val[1],
             desc->peer_id_addr.val[0]);

    ESP_LOGI(BLE_TAG, "conn_itvl = %d conn_latency = %d supervision_timeout = %d "
                "encrypted = %d authenticated = %d bonded = %d",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

static void blecent_log_mbuf(const struct os_mbuf *om){
        uint16_t data_len = OS_MBUF_PKTLEN(om);
        uint8_t *data = malloc(data_len);

        //print_mbuf(attr->om); MODLOG_DFLT(INFO, "[ BLECENT ] END\n\n");

        if (data != NULL) {
            os_mbuf_copydata(om, 0, data_len, data);

            // Log the data using the ESP-IDF logging mechanism
            ESP_LOG_BUFFER_HEX("Notify Data", data, data_len);

            // Free the allocated buffer
            free(data);
        } else {
            ESP_LOGE(BLE_TAG, "Failed to allocate memory for data");
        }
}

static uint8_t *event_data_buffer = NULL;
static void blecent_write_to_outgoing(const uint8_t *event_data){
    int rc;
    MODLOG_DFLT(WARN, "[ BLECENT ] blecent_write_to_outgoind\n");

    if (event_data_buffer != NULL) {
        // Free the previous buffer if allocated
        free(event_data_buffer);
        event_data_buffer = NULL;
    }

    // Ensure event_data is not NULL
    if (event_data == NULL) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] blecent_write_to_outgoing: NULL event_data\n");
        return;
    }

    // Calculate data_len with padding
    uint8_t data_len = event_data[1] + 4;
    // Calculate the number of padding bytes
    size_t padding_bytes = (4 - (data_len % 4)) % 4;
    // Consider padding in the total length
    data_len += padding_bytes;
    // Allocate a new buffer
    event_data_buffer = malloc(data_len);
    if (event_data_buffer == NULL) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Failed to allocate memory for event_data_buffer\n");
        return;
    }

    memcpy(event_data_buffer, event_data, data_len);

    if(data_len < 8){ //at least 8 bytes
        MODLOG_DFLT(ERROR, "[ BLECENT ] blecent_write_to_outgoind too few data\n");
        return;
    }
    //int8_t value[] = {0xFF,0x05,0x00,0x00,0x01,0x0A,0x01,0x00,0x00,0x00,0x00,0x00};
    if(ble_connection_handle == BLE_HS_CONN_HANDLE_NONE || ble_write_handle == BLE_HS_CONN_HANDLE_NONE){
        MODLOG_DFLT(ERROR, "[ BLECENT ] blecent_write_to_outgoind NO HANDLE1\n");
        return;
    }
    //rc = ble_gattc_write_no_rsp_flat(ble_connection_handle, ble_write_handle, event_data, data_len); //NULL = send, wait for ACK, do nothing. no_rsp not working - write have to be with ACK
    rc = ble_gattc_write_flat(ble_connection_handle, ble_write_handle, event_data, data_len, NULL, NULL); //NULL = send, wait for ACK, do nothing. no_rsp not working - write have to be with ACK
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Failed to WRITE characteristic; OutgoingCameraControl_UUID rc = %d", rc);
    }
}

static int blecent_subscribe_to(const struct peer *peer, const ble_uuid_t *service_uuid_to_subscribe, int type){

    MODLOG_DFLT(WARN, "[ BLECENT ] START: blecent_subscribe_to\n");
    int rc;
    const struct peer_chr *chr;

    chr = peer_chr_find_uuid(peer, CameraService_UUID, service_uuid_to_subscribe);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Peer doesn't support characteristic CHR = %s\n", uuid_str(service_uuid_to_subscribe));
        goto err;
    }

    const struct peer_dsc *dsc;

    dsc = peer_dsc_find_uuid(peer, CameraService_UUID, service_uuid_to_subscribe, BLE_UUID16_DECLARE(0x2902)); // BLE_GATT_DSC_CLT_CFG_UUID16
    if (dsc != NULL) {
        MODLOG_DFLT(INFO, "[ BLECENT ] INFO: Service HAVE DESCRIPTOR\n\n  conn_handle = %d\n  conn_handle = %d\n  CHR = %s\n", peer->conn_handle, dsc->dsc.handle, uuid_str(service_uuid_to_subscribe));
        //print_uuid(service_uuid_to_subscribe);
        uint8_t notifyOn[] = {0x01, 0x00};
        switch(type){
            case 0:
                notifyOn[0] = 0x00; // unsubscribe
                break;
            case 1:
                notifyOn[0] = 0x01; // subscribe as notification
                break;
            case 2:
                notifyOn[0] = 0x02; // subscribe as indication (notification with ACK)
                break;
        }

        ESP_LOGW(BLE_TAG,"Subscibe mode type = %d", type);

        /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/

        rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle, notifyOn, sizeof(notifyOn), NULL, NULL); //ble_gattc_write_no_rsp_flat nefunguje https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLERemoteCharacteristic.cpp
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "[ BLECENT ] ERROR: Write to DSC failed\n\n  rc = %d\n  CHR = %s\n", rc, uuid_str(service_uuid_to_subscribe));
            goto err;
        }
    }else{
        MODLOG_DFLT(ERROR, "[ BLECENT ] WARN: Peer doesn't support CCCD, its write only?\n");
    }
    return 0;
err:
    ESP_LOGE(BLE_TAG,"Terminate the connection.");
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_UNSUPPORTED);
}

static void blecent_unsubscribe_subscribe_to(const struct peer *peer, const ble_uuid_t *service_uuid_to_subscribe, int type){
    blecent_subscribe_to(peer, service_uuid_to_subscribe, 0);
    vTaskDelay(2000);
    blecent_subscribe_to(peer, service_uuid_to_subscribe, type);
}

static void blecent_unsubscribe_subscribe_incoming(){
    if(ble_connection_handle == BLE_HS_CONN_HANDLE_NONE){
        MODLOG_DFLT(ERROR, "[ BLECENT ] blecent_update_name NO HANDLE1\n");
        return;
    }
    const struct peer *peer;
    peer = peer_find(ble_connection_handle);
    blecent_unsubscribe_subscribe_to(peer, IncomingCameraControl_UUID, 2);
    return;
}

// have to be here because we need this on update
static void blecent_write_name(const struct peer *peer){
    const struct peer_chr *chr;
    int rc;

    // write to unprotected DeviceName_UUID our name
    chr = peer_chr_find_uuid(peer, CameraService_UUID, DeviceName_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Peer doesn't support DeviceName_UUID\n");
        goto err;
    }else{
        MODLOG_DFLT(WARN, "[ BLECENT ] OK: Peer HAVE DeviceName_UUID\n");

        rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle, esp_device_hostname, strlen(esp_device_hostname), NULL, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Failed to WRITE characteristic; DeviceName_UUID rc = %d\n", rc);
            goto err;
        }
        ble_write_handle = chr->chr.val_handle;
    }
    return;
err:
    MODLOG_DFLT(ERROR, "[ BLECENT ] Error: blecent_write_name failed; conn_handle = %d\n", peer->conn_handle);
    ble_gap_terminate(peer->conn_handle, BLE_ERR_UNSUPPORTED);  
}

static void blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        goto err;
    }

    MODLOG_DFLT(WARN, "[ BLECENT ] Service discovery complete; status = %d conn_handle = %d\n", status, peer->conn_handle);

    // write our name first like ESP-BLE-XXZZYY-255
    blecent_write_name(peer);

    // subscribe to the IncomingCameraControl_UUID
    // 2 means notifications with ACK aka INDICATIONS
    blecent_subscribe_to(peer, IncomingCameraControl_UUID, 2);

    // subscribe to the CameraStatus_UUID
    // 1 means notifications without ACK
    blecent_subscribe_to(peer, CameraStatus_UUID, 1);

    // subscribe to the Timecode_UUID
    // 1 means notifications without ACK
    blecent_subscribe_to(peer, Timecode_UUID, 1);

    // setup Outgoing data
    const struct peer_chr *chr;
    chr = peer_chr_find_uuid(peer, CameraService_UUID, OutgoingCameraControl_UUID);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Peer doesn't support OutgoingCameraControl_UUID\n");
        goto err;
    }else{
        //store outgoing handle as ble_write_handle, just this one we really need as global
        ble_write_handle = chr->chr.val_handle;
    }

    return;
err:
    MODLOG_DFLT(ERROR, "[ BLECENT ] Error: blecent_on_disc_complete failed; status = %d conn_handle = %d\n", status, peer->conn_handle);
    ble_gap_terminate(peer->conn_handle, BLE_ERR_UNSUPPORTED);
}



static int start_peer_discovery(struct ble_gap_event *event, void *arg){
    int rc;
    /*** Go for service discovery after encryption has been successfully enabled ***/
    rc = peer_disc_all(event->connect.conn_handle, blecent_on_disc_complete, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Failed to discover services; rc = %d\n", rc);
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
        ESP_LOGI("BLE_ADV_FIELDS", "Manufacturer Data: Len = %u", fields->mfg_data_len);
        
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
    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        return 0;
    }

    struct ble_hs_adv_fields fields;
    int rc;
    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG,"NO FIELDS ON check_interesting_devices");
        return 0;
    }
    
    int i;
    for (i = 0; i < fields.num_uuids16; i++) {
       // ESP_LOGE(BLE_TAG,"FIELD > (0x%04X)", ble_uuid_u16(&fields.uuids128[i].u));
        if (ble_uuid_u16(&fields.uuids16[i].u) == Service_UUID) {
            //ESP_LOGE(BLE_TAG,"Blackmagic Service_UUID (0x%04X)", ble_uuid_u16(&fields.uuids16[i].u));
            //print_ble_adv_fields(&fields);
            return 1;
        }
    }
    return 0;
}

static void connect_becouse_interesting(ble_addr_t *addr){
    ESP_LOGW(BLE_TAG,"Device is interesting: %s", addr_str(addr->val));
    int rc;
    uint8_t own_addr_type;
    /* Scanning must be stopped before a connection. */
    
    if(ble_gap_disc_active()){
        rc = ble_gap_disc_cancel();
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Failed to cancel scan; rc = %d\n", rc);
            return;
        }
    }


    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] error determining address type; rc = %d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
    //addr = &((struct ble_gap_disc_desc *)disc)->addr;

    rc = ble_gap_connect(own_addr_type, addr, 10000, NULL, blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Failed to connect to device; addr_type = %d addr = %s; rc = %d\n", addr->type, addr_str(addr->val), rc);
        return;
    }
}

static void connect_to_addr(uint8_t *addr){
    return connect_becouse_interesting((ble_addr_t*)addr);
}









static int start_security(struct ble_gap_event *event, void *arg){
    ESP_LOGW(BLE_TAG,"Initiate security");
    /** Initiate security - It will perform
     * Pairing (Exchange keys)
     * Bonding (Store keys)
     * Encryption (Enable encryption)
     * Will invoke event BLE_GAP_EVENT_ENC_CHANGE
     **/
    int rc;
    rc = ble_gap_security_initiate(event->connect.conn_handle);
    if (rc != 0) {
        MODLOG_DFLT(INFO, "[ BLECENT ] Security could not be initiated, rc = %d\n", rc);
        return ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    MODLOG_DFLT(INFO, "[ BLECENT ] Connection secured\n");
    return 0;
}


static void notify_for_new_device(void *disc)
{
    // in this step we will check if PHPR device is interested with check_interesting_devices
    // if so we store ADDR into list and publish data to mqtt (addr and index)
    // TODO ? check device name and send name instead off address?
    ble_addr_t *addr;
    struct ble_gap_disc_desc *disc_;

    disc_ = disc;
    // the device is interesting
    addr = &disc_->addr;
    int8_t rssi = disc_->rssi;
    ESP_LOGI(BLE_TAG,"Device found: %s, RSSI = %d", addr_str(addr->val), (int)rssi);
    // check if device is interesting
    if (rssi < -85) {
        ESP_LOGW(BLE_TAG,"LOW rssi");
        //return;
    }
    // if not return (scanning is still active)
    if (!check_interesting_devices(disc)) {
        return;
    }
    ESP_LOGW(BLE_TAG,"BM Found");
    // Extract and store the device name if available
    //esp_ble_gap_update_adv_data((struct ble_gap_disc_desc *)disc, ESP_BLE_AD_TYPE_NAME_CMPL, discovered_devices[discovered_devices_count].name);
    
    /// Here need to go MQTT message to populate discovered list
    //for testing we execute direct connect
    connect_becouse_interesting(addr);
    return;
}

static void blecent_scan(void)
{
    ESP_LOGW(BLE_TAG,"BLE START SCANNING");
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "[ BLECENT ] error determining address type; rc = %d\n", rc);
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
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error initiating GAP discovery procedure; rc = %d\n", rc);
    }
}

static int blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    //ESP_LOGW(BLE_TAG, "blecent_gap_event");
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        ESP_LOGI(BLE_TAG, "blecent_gap_event BLE_GAP_EVENT_DISC");
        /* Try to connect to the advertiser if it looks interesting. */
        notify_for_new_device(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        esp_ble_state = STATE_CONNECTING;
        BLE_notifyMqtt(0x01);
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "[ BLECENT ] Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            
            //Remember peer. 
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "[ BLECENT ] Failed to add peer; rc = %d\n", rc);
                return 0;
            }
            //store ble_connection_handle globally
            ble_connection_handle = event->connect.conn_handle;
            return start_security(event, arg);
        }

        BLE_notifyMqtt(0x00);
        /* Connection attempt failed; resume scanning. */
        esp_ble_state = STATE_DISCONNECTED;
        MODLOG_DFLT(ERROR, "[ BLECENT ] Error: Connection failed; status = %d\n", event->connect.status);
        //blecent_scan();
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(BLE_TAG, "blecent_gap_event BLE_GAP_EVENT_DISCONNECT");
        /* Connection terminated. */
        //MODLOG_DFLT(INFO, "[ BLECENT ] disconnect; reason = %d ", event->disconnect.reason);
        blecent_print_conn_desc(&event->disconnect.conn);
        //MODLOG_DFLT(INFO, "[ BLECENT ] \n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        if(esp_ble_state == STATE_CONNECTING){
             blecent_scan();
        }
        esp_ble_state = STATE_DISCONNECTED;
        BLE_notifyMqtt(0x00);
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(BLE_TAG, "blecent_gap_event BLE_GAP_EVENT_DISC_COMPLETE");
        MODLOG_DFLT(INFO, "[ BLECENT ] discovery complete; reason = %d\n", event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        //
        struct ble_gap_conn_desc desc;
        int rc;
        //
        ESP_LOGI(BLE_TAG, "blecent_gap_event BLE_GAP_EVENT_ENC_CHANGE encryption change event; status = %d", event->enc_change.status);
        //status have to be 0
        switch(event->enc_change.status){
            case 0:
                // success
                rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
                assert(rc == 0);
                MODLOG_DFLT(INFO, "[ BLECENT ] Encryption has been enabled or disabled for this connection \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                blecent_print_conn_desc(&desc);
                MODLOG_DFLT(INFO, "[ BLECENT ] \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                esp_ble_state = STATE_CONNECTED;
                return start_peer_discovery(event, arg);
            case 14: //status 14 wrong passkey
            case 13: //status 13 passkey timeout
                ESP_LOGE(BLE_TAG, "\n\n\n\nDELETE BONDING\n\n\n\nBLE_GAP_EVENT_ENC_CHANGE status = %d", event->enc_change.status);
                rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
                assert(rc == 0);
                ble_store_util_delete_peer(&desc.peer_id_addr);
                esp_ble_state = STATE_TIMEOUT;
                return BLE_GAP_EVENT_REATTEMPT_COUNT;
            default:
                // non zero and non defined terminate the connection
                /* Forget about peer. */
                peer_delete(event->enc_change.conn_handle);
                //terminate connection
                ble_gap_terminate(event->enc_change.conn_handle, event->enc_change.status);
                break;

        }
        esp_ble_state = STATE_TIMEOUT;
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        //ESP_LOGI(BLE_TAG, "\nblecent_gap_event BLE_GAP_EVENT_NOTIFY_RX");
        //MODLOG_DFLT(INFO, "[ BLECENT ] received %s; conn_handle = %d attr_handle = %d attr_len = %d" , event->notify_rx.indication ? "indication" : "notification", event->notify_rx.conn_handle, event->notify_rx.attr_handle, OS_MBUF_PKTLEN(event->notify_rx.om));
        // Attribute data is contained in event->notify_rx.om. Use os_mbuf_copydata` to copy the data received in notification mbuf
        // Log the data from the notify_rx event
        //blecent_log_mbuf(event->notify_rx.om);
        BLE_onReceive(event->notify_rx.om);
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(BLE_TAG, "blecent_gap_event BLE_GAP_EVENT_MTU");
        MODLOG_DFLT(INFO, "[ BLECENT ] mtu update event; conn_handle = %d cid = %d mtu = %d\n", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REATTEMPT_COUNT:
        //ESP_LOGW("GAP_EVENT", "\n\n\n\n\n\nblecent_gap_event 29 TRY IT AGAIN %d\n\n\n\n\n\n",event->type);
        //i think from my experience that 29 means --- your last try ends with no reason.. if you really want connect do it again
        return BLE_GAP_EVENT_REATTEMPT_COUNT;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
    //case 29:
        
        //ESP_LOGI("GAP_EVENT", "blecent_gap_event BLE_GAP_EVENT_REPEAT_PAIRING");
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
        esp_ble_state = STATE_PASSCODE;
        // Request user input for the 6-digit passcode and convert it to integer
        BLE_notifyMqtt(0x04);
        return 0;
    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        //ESP_LOGI("GAP_EVENT", "BLE_GAP_EVENT_IDENTITY_RESOLVED");
        return 0;    
    case BLE_GAP_EVENT_EXT_DISC:
        //ESP_LOGI("GAP_EVENT", "BLE_GAP_EVENT_EXT_DISC");
        return 0;
    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        //ESP_LOGI("GAP_EVENT", "blecent_gap_event BLE_GAP_EVENT_PHY_UPDATE_COMPLETE %d",event->type);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(BLE_TAG, "subscribe event; conn_handle = %d attr_handle = %d "
                    "reason = %d prevn = %d curn = %d previ = %d curi = %d",
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
    MODLOG_DFLT(ERROR, "[ BLECENT ] Resetting state; reason = %d\n", reason);
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
    ESP_LOGI(BLE_TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

static void ble_stack_deinit_init(void)
{
    // something like restart if needed
    int rc;

    ESP_LOGI(BLE_TAG, "Deinit host");

    rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    } else {
        ESP_LOGE(BLE_TAG, "Nimble port stop failed, rc = %d", rc);
    }

    vTaskDelay(1000); // because blecent_host_task will be executed

    ESP_LOGI(BLE_TAG, "Init host");

    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to init nimble %d ", rc);
        return;
    }

    nimble_port_freertos_init(blecent_host_task);

    ESP_LOGI(BLE_TAG, "BLE INIT DONE");
}

static void ble_stack_deinit(){
    int rc;

    ESP_LOGI(BLE_TAG, "Stop scanning before deinit");
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc = %d\n", rc);
        //return;
    }

    vTaskDelay(1000); // because blecent_host_task will be executed

    ESP_LOGI(BLE_TAG, "Deinit host");

    rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    } else {
        ESP_LOGE(BLE_TAG, "Nimble port stop failed, rc = %d", rc);
    }
}

void blecent_on_reg_callback(struct ble_gatt_register_ctxt * ctxt, void * clb){
    ESP_LOGW(BLE_TAG,"\n\n\n\n\n\n\n\n\n\n\nblecent_on_reg_callback\n\n\n\n\n\n\n\n\n\n\n");
}

void BLEClient_app_main(void)
{
    int rc;

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to init nimble %d ", ret);
        return;
    }

    /* Configure the host. */
    ble_hs_cfg.gatts_register_cb = blecent_on_reg_callback;
    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = BLE_HS_IO_KEYBOARD_ONLY; /// BLE_HS_IO_DISPLAY_YESNO BLE_HS_IO_KEYBOARD_ONLY
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_oob_data_flag = 0;

    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;  
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    //ble_hs_cfg.sm_mitm = 1;
    //ble_hs_cfg.sm_sc = 1;
    //ble_hs_cfg.auth_req = BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM;

    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    rc = ble_svc_gap_device_name_set(esp_device_hostname);
    ESP_LOGW(BLE_TAG, "esp_device_hostname rc=%d", rc);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(blecent_host_task);

    ESP_LOGI(BLE_TAG, "BLE INIT DONE");
}