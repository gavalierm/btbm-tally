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

void update_esp_name(){
    // Ensure id fits within three digits
    if(who_im <= 0 || who_im > 99){
        return;
    }
    snprintf(esp_device_hostname + 8, sizeof(esp_device_hostname) - 8, "%02X%02X%02X-%02d", esp_mac_address[3], esp_mac_address[4], esp_mac_address[5], who_im);
    //ESP_LOGW(APP_TAG, "Updated Hostname: %s\n", esp_device_hostname);
    store_integer_value("who_im", who_im);
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

// Function to get current time in milliseconds
uint64_t millis() {
    return esp_timer_get_time() / 1000ULL;
}

// convert string to array 
// used to merge fixed string with mac address to create ESP HOSTNAME
void stringToUint8Array(const char *str, uint8_t *uint8Array, size_t arraySize) {
    size_t strLength = strlen(str);

    // Ensure the array is large enough to store the converted values
    if (strLength > arraySize) {
        // Handle insufficient array size
        return;
    }

    // Convert each character to its ASCII value
    for (size_t i = 0; i < strLength; ++i) {
        uint8Array[i] = (uint8_t)str[i];
    }
}

// convert HEX data to readable number
// used to convert MQTT buffer to the passkey fo BT
uint32_t hexPayloadToUint32(const char *packet) {
    // Assuming payload is in little-endian order
    const uint8_t *payload = (const uint8_t*)(packet + 8); // Skip the first 8 bytes (headers)
    uint32_t result = 0;

    for (int i = 3; i >= 0; --i) {
        result = (result << 8) | payload[i];
    }

    return result;
}

// convert Fixed16 camera value to integer range 
// used to determine tally luma
float convertFixed16ToFloat(uint8_t* packet) {
    // Extracting payload
    uint16_t payloadValue = (packet[9] << 8) | packet[8];

    // Sign extension for signed 16-bit integer
    int16_t signedValue = (int16_t)payloadValue;

    // Converting to float with scaling factor 2^11
    float floatValue = signedValue / 2048.0;
    ESP_LOGI(APP_TAG, "Converted FLOAT Value (fixed16): %f\n", floatValue);
    return floatValue;
}

// convert Fixed16 camera value to integer range 
// used to determine tally luma
int convertFixed16ToInt(uint8_t* packet) {
    // Extract fixed16 value from the packet
    float floatValue = convertFixed16ToFloat(packet);

    // Convert the float value to an integer in the range [-16, 15]
    int intValue = (int)(floatValue * 255.0);
    ESP_LOGI(APP_TAG, "Converted INT Value (fixed16): %d\n", intValue);
    return intValue;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(APP_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

