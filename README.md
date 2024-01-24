# ESP32 TALLY light with Bluetooth CCU for Blackmagic Camera
The main goal is to use ESP32 with BLE to bridge the gap between Blackmagic ATEM (not part of this project see "App" part) and Blackmagic Pocket camera.


## The Idea
The idea is so simple that I'm really surprised I did not find any GIT repo to do this. 

*The idea is: "do nothing, just bridge."*


### Do Nothing but...
Because we need to do some special stuff like handle BLE discovering, passkey handshakes, registrations, and so on, the PROTOCOL has to be extended to these custom RAW messages. (See PROTOCOL part)


## Requirements
- ESP32 with BLE (RGB shield for tally)
- Python installed on your computer (for running idf.py)
- ESP-IDF installed on your computer (for flashing)
- Custom app to read and send RAW data (like MQTTX)


## TALLY
This project starts as Custom tally light for ATEM many years ago. Using ESP-8266 as Websocket client with RGB shield on. With Node.js websocket server as arbiter for ATEM protocol (based on https://github.com/nrkno/sofie-atem-connection)
So i try to keep this feature in this project too and combine it with BLE.
Tally messages is described in PROTOCOL (custom part RGB tally). Many fancy features out there!

Note: I combined tally messaging with the official CCU protocol using custom unused groups (128,129). Yes it can be done by separate topic, because is just read only, but i like to have just one protocol.

Tip: The brigtness of tally can be changed by official TALLY command. So ...


## How It Works

- ESP boots up with Wifi, BLE, MQTT, and RGB led components
- ESP tries to establish the WIFI connection.
  - if it fails, ESP shuts down (the SSID and PASSCODE can be hardcoded in sdkconfig.defaults or using idf.py Menuconfig)
  - 
- ESP tries to establish MQTT connection
  - if it fails, ESP shuts down (the MQTT host can be hardcoded in sdkconfig.defaults or using idf.py Menuconfig)
  - 
- Now, we have the essential part done. Without wifi and mqtt, we cannot establish a BLE connection because BLE needs more things to do (see BLE part)
- Now tally ligts starts working, because Wifi and MQTT is ready, you can use it as TALLY only BLE is not essential.
- ESP starts scanning and finds the Blackmagic camera (0x1800 SVC id)
  - if found, MQTT will be notified (at this moment, you need to have at least one MQTT client reading the topic to see the camera list)
  - 
- ESP scanning stops and waits for MQTT message indicating which camera you want to connect to
- ESP receives the address from MQTT and performs a connection
- CAMERA will respond with PASSKEY if this is the very first attempt
  - If not and devices are bonded, just accept the connection
  - 
- ESP will notify MQTT with the status code "Need pass" and will wait 30s
  - (this is the camera waiting time and cannot be increased)
  - 
- ESP will wait for MQTT response with a 6-digit passcode
- CAMERA waits for a response, checks the passkey, and accepts the connection
  - if the passkey is wrong, BLE will stop, and you need to start the connect procedure again
  - 
- Now, we have everything set
- CAMERA starts sending CCU, TIMECODE, and THICK data to ESP, and ESP will forward this data to MQTT
- Your client app needs to read the RAW buffer and do what is needed (like converting to human speech)

## Setup


### Setup MQTT client

- You need MQTT client and broker to run this. I use my own node.js MQTT host running on local network, but you can use any public MQTT (but count with latency).
- For testing we can use MQTTX client https://mqttx.app/ (online version http://www.emqx.io/online-mqtt-client#/).
- Run the broker (or use public) and connect your Client app to broker.
- MQTT is ready.


### Topics

Two way communication is done with 2 main topics
```
#define MQTT_DOWNSTREAM_TOPIC "/btmqtt/ccu/raw/downstream"
#define MQTT_UPSTREAM_TOPIC "/btmqtt/ccu/raw/upstream"
```
- Downstream is "From Camera to the broker/mqtt clients"
- Upstream is "From Client to the Camera"

- So Camera is subsribed to the `upstream` and publish data to `downstream`.
- So Client is subsribed to the `downstream` and publish data to `upstream`.

The camera will respond to all `upstream` messages with the same data `downstream`. This is handled by the camera and cannot be turned off. Clients just need to avoid resending `downstream` data back `upstream` to prevent creating an infinite loop.

Using just one topic is not a good idea because the camera will receive its own messages, which is undesirable. This is to avoid "Camera to Camera loops" ensuring that cameras do not receive messages from others.


### Prepare for read and send HEX buffer

Because the data payloads is HEX buffer is not human readable you can not read the data by eyes and need to setup APP for that (see APP part).

But with MQTTX you can send HEX buffer to ESP.

According to the PROTOCOL (custom part Bluetooth) the first message what you will need is "Passkey".

This message have exact structure like common Blackmagic SDI protocol so:

- Destinaton
- Length
- Commands
- Reserved
- Category
- Parameter
- Datatype
- Operation
- Payloads
- Padding (32bit calculated)


```
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 81 03 00 00 01 29 0f 00
```


The payload for "Passkey" is HEX representation of 6 digid number in REVERSED order (LSB) so `123456` will `01 E2 40` in standard order so in LSB `40 E2 01`
Use the rapidtables.com (see Related) to convert digids to hex and do not forget reverse the order. Note: rapidtables.com trim zero from last(first) byte, add `0` if needed.


### App (a.k.a Human client)

The app is not part of this code. If you want to build a good-looking app, just notify me to see how it goes. :)

The main goal is to use some sort of node.js, Svelte, PWA app to receive HEX data, convert it accordingly to the PROTOCOL, and render a nice GUI to display values. And, of course, update values, convert them back to HEX, and send them back to the camera. We want to control the camera, right?


### Auto generated commands for example

```
TESTING: Create datagram from all protocol groups and commands
The fake values are generated with 'value == (max/2)' value.


0 0 Lens Focus
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 00 80 00 00 04 00 00
0 1 Lens Instantaneous autofocus
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 00 01 00 00
0 2 Lens Aperture (f-stop)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 02 80 00 00 40 00 00
0 3 Lens Aperture (normalised)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 03 80 00 00 04 00 00
0 4 Lens Aperture (ordinal)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 04 02 00 80 00 00 00
0 5 Lens Instantaneous auto aperture
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 00 05 00 00
0 6 Lens Optical image stabilisation
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 00 06 00 00
0 7 Lens Set absolute zoom (mm)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 07 02 00 D0 07 00 00
0 8 Lens Set absolute zoom (normalised)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 08 80 00 00 04 00 00
0 9 Lens Set continuous zoom (speed)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 00 09 80 00 00 04 00 00
1 0 Video Video mode
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 01 00 01 00
1 1 Video Gain (up to Camera 4.9)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 01 01 01 00 3F 00 00 00
1 2 Video Manual White Balance
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 01 02 02 00 19 19 00 00
1 3 Video Set auto WB
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 01 03 00 00
1 4 Video Restore auto WB
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 01 04 00 00
1 5 Video Exposure (us)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 01 05 03 00 08 52 00 00
1 6 Video Exposure (ordinal)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 01 06 02 00
1 7 Video Dynamic Range Mode
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 01 07 01 00 01 00 00 00
1 8 Video Video sharpening level
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 01 08 01 00 01 00 00 00
1 9 Video Recording format
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 01 09 02 00
1 10 Video Set auto exposure mode
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 01 0A 01 00 02 00 00 00
1 11 Video Shutter angle
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 01 0B 03 00 50 46 00 00
1 12 Video Shutter speed
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 01 0C 03 00 C4 09 00 00
1 13 Video Gain
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 01 0D 01 00 3F 00 00 00
1 14 Video ISO
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 01 0E 03 00 FF FF FF 3F
1 15 Video Display LUT
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 01 0F 01 00
1 16 Video ND Filter
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 01 10 80 00 00 40 00 00
2 0 Audio Mic level
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 02 00 80 00 00 04 00 00
2 1 Audio Headphone level
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 02 01 80 00 00 04 00 00
2 2 Audio Headphone program mix
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 02 02 80 00 00 04 00 00
2 3 Audio Speaker level
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 02 03 80 00 00 04 00 00
2 4 Audio Input type
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 02 04 01 00 01 00 00 00
2 5 Audio Input levels
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 02 05 80 00 00 00 04 00
2 6 Audio Phantom power
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 02 06 00 00
3 0 Output Overlay enables
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 03 00 00 00
3 1 Output Frame guides style (Camera 3.x)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 03 01 01 00 04 00 00 00
3 2 Output Frame guides opacity (Camera 3.x)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 03 02 80 00 00 04 00 00
3 3 Output Overlays (replaces .1 and .2 above from Cameras 4.0)
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 03 03 01 00 32 32 32 32
4 0 Display Brightness
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 04 00 80 00 00 04 00 00
4 1 Display Exposure and focus tools
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 04 01 00 00
4 2 Display Zebra level
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 04 02 80 00 00 04 00 00
4 3 Display Peaking level
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 04 03 80 00 00 04 00 00
4 4 Display Color bar enable
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 04 04 01 00 0F 00 00 00
4 5 Display Focus Assist
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 04 05 01 00
4 6 Display Program return feed enable
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 04 06 01 00 0F 00 00 00
5 0 Tally Tally brightness
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 05 00 80 00 00 04 00 00
5 1 Tally Front tally brightness
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 05 01 80 00 00 04 00 00
5 2 Tally Rear tally brightness
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 05 02 80 00 00 04 00 00
6 0 Reference Source
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 06 00 01 00 01 00 00 00
6 1 Reference Offset
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 06 01 03 00
7 0 Configuration Real Time Clock
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 07 00 03 00
7 1 Configuration System language
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 07 01 05 00
7 2 Configuration Timezone
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 07 02 03 00
7 3 Configuration Location
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 07 03 04 00
8 0 Color Correction Lift Adjust
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 0C 00 00 08 00 80 00 00 00 00 00 08 00 00 00
8 1 Color Correction Gamma Adjust
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 0C 00 00 08 01 80 00 00 00 00 00 10 00 00 00
8 2 Color Correction Gain Adjust
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 0C 00 00 08 02 80 00 00 00 00 00 40 00 00 00
8 3 Color Correction Offset Adjust
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 0C 00 00 08 03 80 00 00 00 00 00 20 00 00 00
8 4 Color Correction Contrast Adjust
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 08 04 80 00 00 00 08 00
8 5 Color Correction Luma mix
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 08 05 80 00 00 04 00 00
8 6 Color Correction Color Adjust
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 08 06 80 00 00 00 08 00
8 7 Color Correction Correction Reset Default
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 08 07 00 00
10 0 Media Codec
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0A 00 01 00
10 1 Media Transport mode
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0A 01 01 00
10 2 Media Playback Control
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0A 02 01 00
10 3 Media Still Capture
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0A 03 00 00
11 0 PTZ Control Pan/Tilt Velocity
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 0B 00 80 00 00 00 04 00
11 1 PTZ Control Memory Preset
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 0B 01 01 00 02 02 00 00
12 0 Metadata Reel
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 0C 00 02 00 F3 01 00 00
12 1 Metadata Scene Tags
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 01 01 00
12 2 Metadata Scene
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 02 05 00
12 3 Metadata Take
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 0C 03 01 00 31 31 00 00
12 4 Metadata Good Take
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 04 00 00
12 5 Metadata Camera ID
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 05 05 00
12 6 Metadata Camera Operator
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 06 05 00
12 7 Metadata Director
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 07 05 00
12 8 Metadata Project Name
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 08 05 00
12 9 Metadata Lens Type
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 09 05 00
12 10 Metadata Lens Iris
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 0A 05 00
12 11 Metadata Lens Focal Length
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 0B 05 00
12 12 Metadata Lens Distance
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 0C 05 00
12 13 Metadata Lens Filter
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 0D 05 00
12 14 Metadata Slate Mode
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 0C 0E 01 00
128 0 RGB Tally Signaling tally
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 80 00 01 00 04 00 00 00
128 1 RGB Tally Set tally
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 06 00 00 80 01 01 00 00 00 00 00
128 2 RGB Tally Set tally color
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 80 02 81 00 7F 7F 7F 7F
129 0 Bluetooth Notify
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 05 00 00 81 00 01 00 3F 00 00 00
129 1 Bluetooth Disconnect
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 04 00 00 81 01 00 00
129 2 Bluetooth Connect
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 0A 00 00 81 02 81 00 7F 7F 7F 7F 7F 7F 00 00
129 3 Bluetooth Passcode
De Le Cm __ Ca Pa Ty Op 1_ 2_ 3_ 4_ 5_ 6_ 7_ 8_
FF 08 00 00 81 03 03 00 1F A1 07 00
```


## Support and License
This script is free for scenarios such as churches, educational platforms, charity events, etc. If you want to use this script at paid events like concerts, conferences, etc., be grateful and send some "thank you" via [PayPal](https://paypal.me/MarcelGavalier).


## Related
https://github.com/marklysze/Magic-Pocket-Control-ESP32/issues/2

https://github.com/espressif/esp-idf/issues/12989

https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/blecent

https://www.rapidtables.com/convert/number/decimal-to-hex.html

https://documents.blackmagicdesign.com/DeveloperManuals/BlackmagicCameraControl.pdf

https://github.com/nrkno/sofie-atem-connection

Special thanks to @esp-ihr and @rahult-github