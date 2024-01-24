# ESP32 BLE to MQTT Bridge
The main goal is to use ESP32 with BLE to bridge the gap between Blackmagic ATEM (not implemented yet) and Blackmagic Pocket camera.

## The Idea
The idea is so simple that I'm really surprised I did not find any GIT repo to do this. The idea is to "do nothing, just bridge."

### Do Nothing But...
Because we need to do some special stuff like handle BLE discovering, passkey handshakes, registrations, and so on, the PROTOCOL has to be extended to these custom RAW messages. (See PROTOCOL part)

## Requirements
- ESP32 with BLE
- Python installed on your computer (for running idf.py)
- ESP-IDF installed on your computer (for flashing)
- Custom app to read and send RAW data (like MQTTX)

## How It Works
- ESP boots up with Wifi, BLE, MQTT, and RGB led components
- ESP tries to establish the WIFI connection; if it fails, ESP shuts down (the SSID and PASSCODE can be hardcoded in sdkconfig.defaults or using idf.py Menuconfig)
- ESP tries to establish MQTT connection; if it fails, ESP shuts down (the MQTT host can be hardcoded in sdkconfig.defaults or using idf.py Menuconfig)
- Now, we have the essential part done. Without wifi and mqtt, we cannot establish a BLE connection because BLE needs more things to do (see BLE part)
- ESP starts scanning and finds the Blackmagic camera (0x1800 SVC id); if found, MQTT will be notified (at this moment, you need to have at least one MQTT client reading the topic to see the camera list)
- ESP scanning stops and waits for MQTT message indicating which camera you want to connect to
- ESP receives the address from MQTT and performs a connection
- CAMERA will respond with PASSKEY if this is the very first attempt. If not and devices are bonded, just accept the connection
- ESP will notify MQTT with the status code "Need pass" and will wait 30s (this is the camera waiting time and cannot be increased)
- ESP will wait for MQTT response with a 6-digit passcode
- CAMERA waits for a response, checks the passkey, and accepts the connection; if the passkey is wrong, BLE will stop, and you need to start the connect procedure again
- Now, we have everything set
- CAMERA starts sending CCU, TIMECODE, and THICK data to ESP, and ESP will forward this data to MQTT
- Your client app needs to read the RAW buffer and do what is needed (like converting to human speech)

## Support and License
This script is free for scenarios such as churches, educational platforms, charity events, etc. If you want to use this script at paid events like concerts, conferences, etc., be grateful and send some "thank you" via [PayPal](https://paypal.me/MarcelGavalier).
