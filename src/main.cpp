#include <Arduino.h>
#define LOGGING false

#include <Preferences.h>

#include <NeoPixelBus.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <BlueMagicConnection.h>

//#define WIFI_SSID "Reactoo"
//#define WIFI_PASSWORD "turbotron"

#define WIFI_SSID "VideoPP"
#define WIFI_PASSWORD ""

//#define MQTT_HOST IPAddress(192, 168, 0, 102)
#define MQTT_HOST "gavo-macbook-m1.local" //brdige
//#define MQTT_HOST "localhost"
#define MQTT_PORT 1883

#define A_LED 6 //6
#define A_SIZE 7 // Popular NeoPixel ring size
#define B_LED 10 //oproti $
#define B_SIZE 7 // Popular NeoPixel ring size

#define BUTTON_PIN 9

int bt_attempt = 0;

String tally_id = "BTT-YYYY";

//BLEAddress bt_addr = BLEAddress::getLocal();

Preferences PREF;
//BlueMagicConnection BT;
AsyncMqttClient MQTT;

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixel(1, 7);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixel_a(A_SIZE, A_LED);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixel_b(B_SIZE, B_LED);


RgbColor black(0, 0, 0);
RgbColor gray(50, 60, 40);
RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor orange(255, 100, 0);
RgbColor white(255, 255, 255);
RgbColor violet(255, 0, 255);
RgbColor pink(255,50,250);
RgbColor yellow(255, 200, 0);

RgbColor status_color = gray;

RgbColor pixel_last_color = black;
RgbColor pixel_a_last_color = black;
RgbColor pixel_b_last_color = black;

int luminance = 255; //at full

const char* delimiter = ",";

String tpxl_id;
String tpxl_id_front;
String tpxl_id_back;
String tpxl_id_internal;

String btbm_id;
String btbm_id_ccu;
String btbm_id_state;
String btbm_id_state_pin;

void fillPixel(NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> target, RgbColor color, bool show = true){
    for (int i = 0; i < target.PixelCount(); i++) {
        target.SetPixelColor(i, color);
    }
    if( show ){
        target.Show();
    }
}

void do_pixel(RgbColor color){
    if(pixel_last_color == color){
        return;
    }else{
        pixel_last_color = color;
    }

    //luminnance
    //int lumi = round(luminance);
    //pixel.setBrightness(luminance);
    //
    fillPixel(pixel, color);
    //pixel.Show(); 
}

void connectToWifi() {
    do_pixel(white);
    if(LOGGING) Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        if(LOGGING) Serial.println("WiFi connected");
        if(LOGGING) Serial.println("IP address: ");
        if(LOGGING) Serial.println(WiFi.localIP());
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        if(LOGGING) Serial.println("WiFi lost connection");
        break;
    case ARDUINO_EVENT_WIFI_READY:
        if(LOGGING) Serial.println("wifi ready");
        break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        break;
    default:
        if(LOGGING) Serial.printf("[WiFi-event] event: %d\n", event);
      break;
    }
}


void connectToBt(){
    if(LOGGING) Serial.println("Thick to BT...");
    do_pixel(blue);
    if(MQTT.connected() and WiFi.isConnected()){
        //return;
        //if(!BT.isConnected()){
            if(LOGGING) Serial.println("Connecting to BT...");
            //BT.scan();
        //}
    }
}

void onBtConnect() {
    if(LOGGING) Serial.println("Connected to BT.");
    bt_attempt = 0;
}

void onBtDisconnect(){
    if(LOGGING) Serial.println("Disconnected to BT.");
    bt_attempt++;
    if(bt_attempt < 3){
        vTaskDelay(5000);
        //connectToBt();      
    }
}

void connectToMqtt() {
    do_pixel(orange);
    if (WiFi.isConnected()) {
        if(MQTT.connected()){
            MQTT.disconnect();
        }else{             
            if(LOGGING) Serial.println("MQTT: Connecting to MQTT...");
            MQTT.connect();
        }
    }else{
        if(LOGGING) Serial.println("MQTT: Waiting for Wifi");
    }
    
}

void onMqttConnect(bool sessionPresent) {

    if(LOGGING) Serial.println("Connected to MQTT.");

    MQTT.subscribe(tpxl_id.c_str(), 0);
    MQTT.subscribe(tpxl_id_front.c_str(), 0);
    MQTT.subscribe(tpxl_id_back.c_str(), 0);
    MQTT.subscribe(tpxl_id_internal.c_str(), 0);

    //subscribe to ccu
    MQTT.subscribe(btbm_id_ccu.c_str(), 0);

    //need subscribe to receive the PIN
    MQTT.subscribe(btbm_id_state_pin.c_str(), 0);

}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    if(LOGGING) Serial.println("Disconnected from MQTT.");
    vTaskDelay(5000);
    connectToMqtt();
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
    return;
}

void onMqttUnsubscribe(uint16_t packetId) {
    return;
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total) {

    //do_pixel(orange);
    if(LOGGING) {
        Serial.print("MQTT: Got message ");
        Serial.print(index);
        Serial.print(" ");
        Serial.print(topic);
        Serial.print(" ");
        Serial.print(payload);
        Serial.println();
    }

    String topic_str = String(topic);

    int target = 0;
    if( topic_str == tpxl_id ){
        target = 1;
    }else if( topic_str == tpxl_id_front ){
        target = 2;
            fillPixel(pixel_a, red);
            //pixel_a.Show();

            //fillPixel(trg, red);
            //pixel_b.Show();
    }else if( topic_str == tpxl_id_back ){
        target = 3;
            fillPixel(pixel_a, green);
            //pixel_a.Show();

            //fillPixel(trg, green);
            //pixel_b.Show();
    }else if( topic_str == tpxl_id_internal ){
        target = 4;
    }

    return;

    if( target > 0 ){
        //tpxl
        if(LOGGING) Serial.print("Tally message ");
        if(LOGGING) Serial.println(target);
        if (length < 6) {
            return;
        }

        char* colorStringHex = NULL;
        if (payload[0] == '#') {
          colorStringHex = strtok(payload, "#"); // remove brackets
        } else {
          colorStringHex = payload;
        }

        int r = 0;
        int g = 0;
        int b = 0;

        unsigned long token = strtoul(colorStringHex, NULL, 16); // + 1 parse as Hex, skipping the leading '#'
        r = (token >> 16) & 0xFF;
        g = (token >> 8) & 0xFF;
        b = token & 0xFF;

        RgbColor cl(r, g, b);

        if(target == 1){
            if(pixel_a_last_color == cl and pixel_b_last_color == cl){
                if(LOGGING) Serial.println("Same color");
                //return;
            }
            pixel_a_last_color = cl;
            pixel_b_last_color = cl;
            //fill color
            fillPixel(pixel_a, cl);
            //pixel_a.Show();

            fillPixel(pixel_b, cl);
            //pixel_b.Show();
            return;
        }

        if(target == 2){
            if(pixel_a_last_color == cl){
                return;
            }  
            //fill color
            fillPixel(pixel_a, cl);
            //Show
            //pixel_a.Show();

            pixel_a_last_color = cl;
            return;          
        }

        if(target == 3){
            if(pixel_b_last_color == cl){
                return;
            } 
            //fill color
            fillPixel(pixel_b, cl);
            //Show
            //pixel_b.Show(); 

            pixel_b_last_color = cl;
            return;          
        }

        if(target == 4){
            if(pixel_last_color == cl){
                return;
            }  
            //fill color
            fillPixel(pixel, cl);
            //Show
            //pixel.Show();

            pixel_last_color = cl;
            return;          
        }


    /**





    String topic_str = String(topic);
    //const char* const_topic = (const char*) topic;
    if(topic_str.indexOf("tpxl/" + tally_id + "") != -1){

        if(LOGGING) Serial.println("Tally message");
        //if(LOGGING) Serial.println(length);

        if (length < 6) {
            return;
        }

        //char* payload = "[#ff0000 , #ff0000 , #ff0000 , #ff0000 , #ff0000]";
        char* colorString = NULL;
        if (payload[0] == '[') {
          colorString = strtok(payload, "[]"); // remove brackets
        } else {
          colorString = payload;
        }

        int target = 0;

        if( topic_str == "tpxl/" + tally_id + "/front" ){
            if(LOGGING) Serial.println("Tally FRONT");
            target = 1;
        }else if( topic_str == "tpxl/" + tally_id + "/rear" ){
            if(LOGGING) Serial.println("Tally REAR");
            target = 2;
        }else if( topic_str == "tpxl/" + tally_id + "/internal" ){
            if(LOGGING) Serial.println("Tally INTERNAL");
            target = 3;
        }

        int i = 0;
        int r = 0;
        int g = 0;
        int b = 0;

        RgbColor cl(0, 0, 0);

        //starting at black
        if(target == 0 or target == 1){
            fillPixel(trg, cl);
        }
        if(target == 0 or target == 2){
            //fillPixel(trg, cl);
        }
        if(target == 3){
          fillPixel(trg, cl);
        }

        for (char* color = strtok(colorString, delimiter); color != NULL; color = strtok(NULL, delimiter)) {
            
            if (strlen(color) > 2) {

                if (color[0] == '#') {
                  color = strtok(color, "#"); // remove brackets
                } else {
                  color = color;
                }

                unsigned long token = strtoul(color, NULL, 16); // + 1 parse as Hex, skipping the leading '#'
                r = (token >> 16) & 0xFF;
                g = (token >> 8) & 0xFF;
                b = token & 0xFF;
            }else{
                r = 0;
                g = 0;
                b = 0;
            }
            
            cl(r, g, b);

            if((target == 0 or target == 1) && i < A_SIZE){
                pixel_a.setPixelColor(i, cl);
            }
            if((target == 0 or target == 2) && i < B_SIZE){
                //pixel_b.setPixelColor(i, cl);
            }
            if(target == 3 && i < 1){
                pixel.setPixelColor(i, cl);
            }

            i++;
        }

        if(i == 1){
            //simple pixel
            if(target == 0 or target == 1){
                fillPixel(trg, cl);
            }
            if(target == 0 or target == 2){
                //fillPixel(trg, cl);
            }
            if(target == 3){
              fillPixel(trg, cl);
            }
        }

        if(target == 0 or target == 1){
            pixel_a.Show();
        }
        if(target == 0 or target == 2){
            //pixel_b.Show();
        }
        if(target == 3){
            pixel.Show();
        }
        
        **/

        /**

        const char* const_payload = (const char*) payload;

        long rgb = strtol(const_payload + 1, 0, 16); // parse as Hex, skipping the leading '#'
        int r = (rgb >> 16) & 0xFF;
        int g = (rgb >> 8) & 0xFF;
        int b = rgb & 0xFF;

        RgbColor cl(r, g, b);
        //
        if( topic_str == "tpxl/" + tally_id + "/front" ){
            if(LOGGING) Serial.println("Tally FRONT");
            do_pixel_a(cl);
        }else if( topic_str == "tpxl/" + tally_id + "/rear" ){
            if(LOGGING) Serial.println("Tally REAR");
            do_pixel_b(cl);
        }else{
            do_pixel_a(cl);
            do_pixel_b(cl);
            if(LOGGING){
                die = false;
                do_pixel(cl);
            }
        }
        **/
        return;
    }

    if( topic_str == btbm_id_ccu ){
        if(LOGGING) Serial.println("CCU message");
        if (length < 10) {
            return;
        }

        /**
        if(BT.isConnected()){
            //
            char* token = strtok(payload + 1, delimiter);  // + 1 Skip the first '{' character
            size_t i = 0;
            uint8_t* pData = new uint8_t[length];
            while (token != NULL && i < length - 1) {  // -1 Skip the first '{' character
                uint8_t value = 0;
                if (strncmp(token, "0x", 2) == 0) {  // Check if token starts with "0x"
                    value = static_cast<uint8_t>(strtoul(token + 2, NULL, 16));  // Convert token to hex value
                } else {
                    value = static_cast<uint8_t>(atoi(token));  // Convert token to decimal value
                }
                if(LOGGING) Serial.println(value);
                pData[i] = value;
                i++;
                token = strtok(NULL, delimiter);
                //value = =round(((range + 2047)/2048) * (2048 * P2))
            }

            BT.send(pData,i);
            // Free the dynamically allocated memory
            delete[] pData;
        }

        **/
        return;
    }

    if( topic_str == btbm_id_state_pin ){
        do_pixel(red);
        //BT.setPin(atoi(payload));
        return;
    }

}

void onMqttPublish(uint16_t packetId) {
    return;
}

/**
void onBtStateChange(int state, const char * state_char){
    if(MQTT.connected()){
        MQTT.publish(btbm_id_state.c_str(), 0, true, state_char);
    }

    if(state == BMC_PAIRING_PIN_REQUEST){
        //5 means pin request
        if(LOGGING){
            Serial.println("---> PLEASE ENTER 6 DIGIT PIN (end with ENTER) : ");
            int pinCode = 0;
            char ch;

            unsigned long start_millis = millis();
            while((ch != '\n') && (millis() - start_millis < 20000)){
                  ch = Serial.read();
                  if (ch >= '0' && ch <= '9'){
                    pinCode = pinCode * 10 + (ch - '0');
                    //Serial.print(ch);
                  }
            }
            if(pinCode == 0){
                pinCode = 1;
            }
            //BT.setPin(pinCode);
        }
    }
}

static void receiveNotify(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{

    if (length < 4) {
        // Invalid packet, ignore it
        return;
    }

    //skip group 9 is not documented and i do not know what it is
    if(pData[4] == 9){
        return;
    }
    //

    if(LOGGING) Serial.println("CONTROL: Publish change");

    char buffer[length * 4];
    int pos = 0;
    buffer[pos++] = '[';
    for (int i = 0; i < length; i++) {
      if (pData[i] == 0) {
        pos += sprintf(&buffer[pos], "0,");
      } else {
        pos += sprintf(&buffer[pos], "%d,", pData[i]);
      }
    }
    if (pos > 0) {
      buffer[pos - 1] = ']'; // Replace the last comma with null terminator
    }
    buffer[pos++] = '\0';

    //const char* str = reinterpret_cast<const char*>(pData);
    MQTT.publish(("btbm/" + tally_id + "/ccu/change").c_str(), 0, true, buffer);

}

**/

void clearPairing(){
    do_pixel(red);
    //BT.clearPairing();
    vTaskDelay(1010);
    do_pixel(green);
    vTaskDelay(1010);
    do_pixel(status_color);
}


void setup(){


    if(LOGGING) Serial.begin(115200); delay(2000); //Waiting for serial;
    if(LOGGING) Serial.setDebugOutput(false);
    if(LOGGING) Serial.println(".");
    if(LOGGING) Serial.println(".");
    if(LOGGING) Serial.println(".");

    delay(2000);

    //wifiTask = xTimerCreate("wifiTask", pdMS_TO_TICKS(50000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
    //mqttTask = xTimerCreate("mqttTask", pdMS_TO_TICKS(5000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    //btTask = xTimerCreate("btTask", pdMS_TO_TICKS(5000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToBt));


    pinMode(BUTTON_PIN, INPUT_PULLUP);

    /**
     * 
     *  ID
     * 
     **/
    // Get the chip ID
    String aa = WiFi.macAddress().substring(9, 11);  // Extract AA as a string
    String bb = WiFi.macAddress().substring(12, 14); // Extract BB as a string
    String chipId = aa + bb;  

    Serial.print("Chip ID: "); Serial.println(chipId);
    
    PREF.begin("tally", false);

    if(chipId){
        tally_id.replace("YYYY", chipId);
        PREF.putBool("tally_id_reset", true);
    }
    

    if(LOGGING){
        Serial.print("tally_id ");
        Serial.print(tally_id);
        Serial.println();
    }

    if(PREF.getBool("tally_id_reset", false)){
        PREF.putString("tally_id", tally_id);   
        PREF.putBool("tally_id_reset", false);   
    }
    tally_id = PREF.getString("tally_id", tally_id);  
    PREF.putString("tally_id", tally_id);
    PREF.end();

    if(LOGGING){
        Serial.print("PREF: tally_id ");
        Serial.print(tally_id);
        Serial.println();
    }

    tpxl_id = "tpxl/" + tally_id;

    tpxl_id_front = tpxl_id + "/front";

    tpxl_id_back = tpxl_id + "/back";

    tpxl_id_internal = tpxl_id + "/internal";

    btbm_id = "btbm/" + tally_id;

    btbm_id_ccu = btbm_id + "/ccu";

    btbm_id_state = btbm_id + "/state";

    btbm_id_state_pin = btbm_id + "/state/pin";

    /**
     * 
     *  PIXEL
     * 
     **/
    if(LOGGING) Serial.println("PIXEL: Init");
    pixel.Begin();
    pixel_a.Begin();
    pixel_b.Begin();

    PREF.begin("tally", false);
    luminance = PREF.getInt("luminance", 255);
    PREF.putInt("luminance", luminance);
    PREF.end();

    //pixel.setBrightness(luminance);
    fillPixel(pixel, status_color);
    //pixel.Show(); 

    //pixel_a.setBrightness(luminance);
    fillPixel(pixel_a, status_color);
    //pixel_a.Show(); 

    ////pixel_b.setBrightness(luminance);
    //fillPixel(pixel_b, status_color);
    //pixel_b.Show(); 

    /**
     * 
     *  WiFi
     * 
     **/
    do_pixel(pink);
    if(LOGGING) Serial.println("WiFi: Init");
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);

    if(LOGGING) Serial.printf("Default wifi_hostname: %s\n", WiFi.getHostname());

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while(!WiFi.isConnected()){
        vTaskDelay(1010);
    }

    /**
     * 
     *  MQTT
     * 
     **/
    if(LOGGING) Serial.println("MQTT: Init");
    do_pixel(orange);
    MQTT.setClientId(tally_id.c_str());
    MQTT.onConnect(onMqttConnect);
    MQTT.onDisconnect(onMqttDisconnect);
    //MQTT.onSubscribe(onMqttSubscribe);
    //MQTT.onUnsubscribe(onMqttUnsubscribe);
    MQTT.onMessage(onMqttMessage);
    //MQTT.onPublish(onMqttPublish);
    MQTT.setServer(MQTT_HOST, MQTT_PORT);
    MQTT.connect();

    while(!MQTT.connected()){
        vTaskDelay(1010);
    }

    /**
     * 
     *  BT
     * 
     **/
    if( digitalRead(BUTTON_PIN) == LOW and true){
        do_pixel(red);
    }else{
        do_pixel(blue);

        if(LOGGING) Serial.println("BT: Init");
        //BT.setHostname(tally_id);
        //BT.setPreferences(PREF);
        //BT.onStateChange(onBtStateChange);
        //BT.onConnect(onBtConnect);
        //BT.onDisconnect(onBtDisconnect);
        //BT.onReceive(receiveNotify);

        //BT.begin();

        //while(BT.isScanning()){
        //    vTaskDelay(1010);
        //}
    }

    
    if(LOGGING) Serial.println("Starting");

}

unsigned long g_millis = millis();
bool btn_last_state = HIGH;
uint8_t play[9] = {255,5,0,0,4,4,0,0,0};

void onRelease(){
    if(btn_last_state == HIGH and digitalRead(BUTTON_PIN) == LOW ){
        btn_last_state = LOW;
        vTaskDelay(10);
        do_pixel(yellow);
        g_millis = millis();

        /**
        if(BT.isConnected()){
            
            if(play[8]){
                play[8] = 0;
            }else{
                play[8] = 30;
            }
            BT.send(play,12);
        }else{
            connectToBt();
        }
        **/
    }

    if(btn_last_state == LOW and digitalRead(BUTTON_PIN) == LOW and (millis() - g_millis) > 10000){
        //reset counter to new
        btn_last_state = HIGH;

        if(LOGGING) Serial.println("MAIN: Holded");
        //if(!BT.isScanning()){
        //    clearPairing();
        //}
    }

    if(btn_last_state == LOW and digitalRead(BUTTON_PIN) == HIGH){
        g_millis = millis();
        btn_last_state = HIGH;
    }

}


unsigned long thick_millis = millis();
bool thick = false;
void loop(){
    //onRelease();

    if(thick && (millis() - thick_millis > 100)){
        //do_pixel(black);
        thick = false;
    }else if(millis() - thick_millis > 5000){
        thick_millis = millis();
        //do_pixel(green);
        //BT.heartBeat();  
        thick = true;
    }
    /**
    if(millis() - start_millis > 2000){ 
        //start_millis = millis();
        if(LOGGING) Serial.println("Remaining memory");
        //uint32_t* freeMem;
        //freeMem = ESP.getFreeHeap();
        if(LOGGING) Serial.println(freeMem);
        if(LOGGING) Serial.println(configMINIMAL_STACK_SIZE);
    }
    **/
    delay(1);
}