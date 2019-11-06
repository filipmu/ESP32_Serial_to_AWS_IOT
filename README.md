# ESP32 Serial to AWS_IOT Bridge

This firmware allows an ESP32 (ESP32 Devkitc) to act as a kind of bridge between other embedded systems and AWS IOT.  This is useful for systems that do not have wifi access.  Its simply a matter of having the embedded system output the data in simple JSON format on the serial line, with the header "MQTT:".  The message content( subtopics, JSON) and message timing are under the control of the embedded system code.  The ESP32 adds a proper time stamp (based on unix time) and handles the security with AWS IOT.

 The code is based on the great code from Debashish Sahu https://github.com/debsahu/ESP-MQTT-AWS-IoT-Core with additions
 to handle the additional serial IO RX line and MQTT message construction.
 
 ## How it works
 The ESP32 receives json messages via serial UART1 and then sends them off to the AWS MQTT broker.

1. ESP32 monitors an additional serial port (configured to pin 12 on ESP32 devkit C) for any messages starting with "MQTT:"  For example a line like:
         MQTT:/embedded_system/data{"element1":32,"element2":"yes","element3",54.2}

2. The line is parsed, a time stamp is added, and it is sent to AWS_IOT as the following:
         $aws/things/THINGNAME/shadow/update/embedded_system/data{"ts":1510592825,"element1":32,"element2":"yes","element3",54.2}

3. In addition the ESP32 will also send a regular heartbeat message, containing the time and the WIFI rssi signal strength.  This can be useful for testing/debugging.

The standard ESP32 USB serial IO is there as well (UART0), and so a serial monitor can be used to see the message activity and ESP32 status.

Note that the ESP32 is NOT 5v tolerant - only 3.3 volt max on the serial IO lines.  So a level shifter or simple voltage divider is needed. https://icircuit.net/arduino-interfacing-arduino-uno-esp32/2134
