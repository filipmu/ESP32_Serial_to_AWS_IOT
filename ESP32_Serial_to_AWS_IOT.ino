// ESP32 Serial to AWS_IOT Bridge
//
// This firmware allows an ESP32 to act as a kind of bridge between other embedded systems and AWS IOT.
// This is useful for systems that do not have wifi access.  Its simply a matter of having the embedded
// system output the data in simple JSON format on the serial line.
//
// The code is based on the great code from Debashish Sahu https://github.com/debsahu/ESP-MQTT-AWS-IoT-Core with additions
// to handle the serial IO and MQTT message construction.
// 
// How it works
// The ESP32 receives json messages via serial UART and then sends them off to the AWS MQTT broker.
//
// 1. ESP32 monitors an additional serial port for any messages starting with "MQTT:"  For example a line like:
//         MQTT:/embedded_system/data{"element1":32,"element2":"yes","element3",54.2}
//
// 2. The line is parsed, a time stamp is added, and it is sent to AWS_IOT as the following:
//         $aws/things/THINGNAME/shadow/update/embedded_system/data{"ts":1510592825,"element1":32,"element2":"yes","element3",54.2}
//
// 3. In addition the ESP32 will also send a regular heartbeat message, containing the time and the WIFI rssi signal strength       

#include <WiFi.h>
#include <WiFiClientSecure.h>

#define MQTT_MAX_PACKET_SIZE 2048  //Tweak defines for PubSubClient
#define MQTT_KEEPALIVE 20  //Tweak defines for PubSubClient
#include <PubSubClient.h>
#include <HardwareSerial.h>


#include <time.h>
#include <TimeLib.h>

#define emptyString String()

//Amazon IOT setup - Follow instructions from https://github.com/debsahu/ESP-MQTT-AWS-IoT-Core/blob/master/doc/README.md
//Enter values in secrets.h ▼
#include "secrets.h"

//In addition to sending the data received by the serial port, send a regular 'heartbeat' test message 
//with the time and the WiFi signal strength
#define MQTT_SERIAL_PUBLISH_TEST_CH "/esp32/test/" //test sub-topic
#define TEST_PUB_INTERVAL_MS 5000  //interval in ms for sending the test message

time_t currentTime = 0,startTime=0;
int status;


long unsigned int msec=0;
long rssi;

const int MQTT_PORT = 8883;
const char MQTT_SUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";
const char MQTT_PUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";

#ifdef USE_SUMMER_TIME_DST
uint8_t DST = 1;
#else
uint8_t DST = 0;
#endif

WiFiClientSecure net;


PubSubClient client(net);

HardwareSerial Serial_one(1);

unsigned long lastMillis = 0;
time_t current_t;
time_t nowish = 1510592825;

void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, DST * 3600, "pool.ntp.org", "time.nist.gov");
  current_t = time(nullptr);
  while (current_t < nowish)
  {
    delay(500);
    Serial.print(".");
    current_t = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&current_t, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void pubSubErr(int8_t MQTTErr)
{
  if (MQTTErr == MQTT_CONNECTION_TIMEOUT)
    Serial.print("Connection tiemout");
  else if (MQTTErr == MQTT_CONNECTION_LOST)
    Serial.print("Connection lost");
  else if (MQTTErr == MQTT_CONNECT_FAILED)
    Serial.print("Connect failed");
  else if (MQTTErr == MQTT_DISCONNECTED)
    Serial.print("Disconnected");
  else if (MQTTErr == MQTT_CONNECTED)
    Serial.print("Connected");
  else if (MQTTErr == MQTT_CONNECT_BAD_PROTOCOL)
    Serial.print("Connect bad protocol");
  else if (MQTTErr == MQTT_CONNECT_BAD_CLIENT_ID)
    Serial.print("Connect bad Client-ID");
  else if (MQTTErr == MQTT_CONNECT_UNAVAILABLE)
    Serial.print("Connect unavailable");
  else if (MQTTErr == MQTT_CONNECT_BAD_CREDENTIALS)
    Serial.print("Connect bad credentials");
  else if (MQTTErr == MQTT_CONNECT_UNAUTHORIZED)
    Serial.print("Connect unauthorized");
}

void connectToMqtt(bool nonBlocking = false)
{
  Serial.print("MQTT connecting ");
  while (!client.connected())
  {
    if (client.connect(THINGNAME))
    {
      Serial.println("connected!");
      if (!client.subscribe(MQTT_SUB_TOPIC))
        pubSubErr(client.state());
    }
    else
    {
      Serial.print("failed, reason -> ");
      pubSubErr(client.state());
      if (!nonBlocking)
      {
        Serial.println(" < try again in 5 seconds");
        delay(5000);
      }
      else
      {
        Serial.println(" <");
      }
    }
    if (nonBlocking)
      break;
  }
}

void connectToWiFi(String init_str)
{
  if (init_str != emptyString)
    Serial.print(init_str);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  if (init_str != emptyString)
    Serial.println("ok!");
}

void checkWiFiThenMQTT(void)
{
  connectToWiFi("Checking WiFi");
  connectToMqtt();
}

unsigned long previousMillis = 0;
const long interval = 5000;

void checkWiFiThenMQTTNonBlocking(void)
{
  connectToWiFi(emptyString);
  if (millis() - previousMillis >= interval && !client.connected()) {
    previousMillis = millis();
    connectToMqtt(true);
  }
}

void checkWiFiThenReboot(void)
{
  connectToWiFi("Checking WiFi");
  Serial.print("Rebooting");
  ESP.restart();
}




void publishSerialData2(String & topic, String & data){
  if (!client.connected()) {
    //reconnect();
    connectToMqtt(true);
  }
  
  topic = MQTT_PUB_TOPIC + topic;  //add Amazon header every time
  status = client.publish((char*) topic.c_str(), (char*) data.c_str());
  if(status==0)
    Serial.print('Pub failure:');
  Serial.print("Topic:");
  Serial.print(topic);
  Serial.print(" Message:");
  Serial.println(data);
}


void zero_pad2(String & s, int i){
  if (i<10) s+= "0";
  s += String(i);
  
}

void time_and_date_string(String & s, time_t t)
// Turn Unix time into a time and date string
{
     zero_pad2(s,hour(t));
     s += ":";
     zero_pad2(s,minute(t));
     s += ":";
     zero_pad2(s,second(t));
     s += " ";
     zero_pad2(s,month(t));
     s += "/";
     zero_pad2(s,day(t));
     s += "/";
     s +=  String((year(t))) ;      
}




void setup()
{
  Serial.begin(115200);

// initialize another serial port, with input on pin 12.
  Serial_one.begin(115200, SERIAL_8N1, 12, 13);  //RX pin 12, TX pin 13
  
  delay(1000);
  Serial.println();
  Serial.println();

  WiFi.setHostname(THINGNAME);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  connectToWiFi(String("Attempting to connect to SSID: ") + String(ssid));
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal strength (RSSI):");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  NTPConnect();

  net.setCACert(cacert);
  net.setCertificate(client_cert);
  net.setPrivateKey(privkey);


  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(messageReceived);

  connectToMqtt();
}

void loop()
{
  //current_t = time(nullptr);
  if (!client.connected())
  {
    checkWiFiThenMQTT();
  }
  else
  {
    client.loop();

    while (Serial_one.available() > 0) {
      String req = Serial_one.readStringUntil('\n');  //read first line

      
      if (req.startsWith("MQTT:")){ //look for "MQTT:"
        req = req.substring(5);  // get rid of "MQTT:"
        String tad = String();
        int m_place = req.indexOf("{"); //find the start of the JSON
        if(m_place<0) Serial.println("Message error: no '{' found");
        if(m_place<2) Serial.println("Topic error: no topic sent");
        String subtop = req.substring(0,m_place-1); //get subtopic
        req=req.substring(m_place) ; //keep message        
        req.trim();
        String response = String(); //construct new JSON
        response = response + "{\"ts\":";  //add the timestamp key
        response = response +time(nullptr)+",";  //add the current time as value
        response = response +req.substring(1);  //append the rest of the JSON from the serial input
        publishSerialData2(subtop, response); //publish to MQTT
          
      }
    }


    
    if (millis() - lastMillis > TEST_PUB_INTERVAL_MS)  //check if its time to publish a heartbeat
    {
      lastMillis = millis();
      String response = String("{\"Time\":\"");
  
      time_and_date_string(response, time(nullptr)); 
      response = response + "\",";
      rssi = WiFi.RSSI();
      response = response +"\"Signal strength (RSSI dBm)\":" + String(rssi) +"}";
      String subtop="/esp32/test/";
      publishSerialData2(subtop, response);
      
    }
  }
}
