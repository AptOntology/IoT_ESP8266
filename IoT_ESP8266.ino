/**
  ESP8266 FLASH ROM
  Settings:
    SETTING=analog,PIN=0,ENABLE=true,Threshold=120000,LASTRUN=0,
    SETTING=digital,PIN=12,ENABLE=true,Threshold=60000,LASTRUN=0,
  Actions:
    ACTION=restart
    ACTION=setpin,12,high,
*/
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "Adafruit_Sensor.h"
#include "DHT.h"
#include "DHT_u.h"
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>

// Global Definitions  //
#define ROM_VERSION "VER=01" // ROM VERSION 
#define COMMAND_HOST "YourMqttServer.com"
#define MQTT_PORT 8883
#define MQTT_USER "esp"
#define MQTT_PASS "pass"

WiFiClientSecure client;
Adafruit_MQTT_Client mqtt(&client, COMMAND_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);

// Local Function variables //
// ======================================== //
String chipID = (String)ESP.getChipId();
String homeWifiPass = (String)"password";

//WIFI
const char* offlineWifi[30]; // bad access points
const char* onlineWifi[30]; // good access points
int connectWifiCount; //ConnectWifi()

// Control
bool boolRunOnceOnWifi = true;
unsigned long startTime;

// Commands and settings
String espSettings[20]; //Holds global settings
String arSetting[20]; //CommaStrToArray global temporary variable
int rInt; //CommaStrToArray

// Subscribe -- Setup(), SubscriptMqtt()
// ======================================== //
String mqttPubControlStr = "sensornet/" + chipID + "/control";
Adafruit_MQTT_Subscribe subscribeControl = Adafruit_MQTT_Subscribe(&mqtt, mqttPubControlStr.c_str());
Adafruit_MQTT_Subscribe subscribeM2M = Adafruit_MQTT_Subscribe(&mqtt, "m2m");

// Function definitions //
// ======================================== //
void OnMqttConnected(); //Everything control does while connected to WIFI and given control instructions
// MQTT
bool MqttConnect();
void MqttPublishConnected();
void MqttPublishAnalogPin(int pinNumber);
void MqttPublishDigitalPin(int pinNumber);
void MqttPublishDht(int dhtType,int pinNumber);
void MqttPublish(String strTopic, String strToPublish);
void MqttSubscribe();
void MqttReceived(char * myReceivedMqtt);
// Updates
void UpdateSetting(String line);
void GetUpdate(String host = COMMAND_HOST);
void UpdateROM(String binName);
// HTTP Server
void handleRoot();
void handleScanNetworks();
void handleConnectWifi();
void handleControl();
// WIFI
bool ConnectWifi(String ap, String pass);
bool ConfirmOfflineWifi(const char* strWifi);
void SetOfflineWifi(const char* strWifi);
bool ConfirmOnlineWifi(const char* strWifi);
void SetOnlineWifi(const char* strWifi);
// ETC
String GetPublicIP(String host);
void Blink(int count, int interval);
void Out(String function, String message);
void DoLove(); //Delay
//DigitalPulse
void PulseStart(int pinNumber, bool motionOnHigh);
void DigitalPulse(int pinNumber, bool motionOnHigh);
//AP
void doHandleClient();
void SetupAP();
int ledPin = 2;

void setup() {
  Serial.begin(115200);
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(500);
  }

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);    // turn the LED off

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  //subscribe to MQTT needs to be run only once
  mqtt.subscribe(&subscribeControl);
  mqtt.subscribe(&subscribeM2M);

  //init on/offlineWifi Array
  for (int i = 0; i < (sizeof(offlineWifi) / sizeof(offlineWifi[0])); i++)
  {
    onlineWifi[i] = offlineWifi[i] = "";
  }
  
  SetOnlineWifi("ESP"); //home wifi
}

void loop() {
  startTime = millis(); //Time to publish MQTT
  if (WiFi.status() != WL_CONNECTED)
  {
    Out("Control", "Begin Connect");
    digitalWrite(ledPin, HIGH); //Turn LED OFF

    //Connect and get update
    if (ConnectWifi("", ""))
      GetUpdate();
  }
  else
  {
    while (WiFi.status() == WL_CONNECTED)
    {
      if (MqttConnect())
      {
        OnMqttConnected();
      }
      else
      {
        //Connected to Wifi, Not Online
        if (ConfirmOnlineWifi(WiFi.SSID().c_str()) == false)
        {
          SetOfflineWifi(WiFi.SSID().c_str());
          boolRunOnceOnWifi = true;
        }
        Blink(2, 250);
      }
    }
  }
  Out("Loop", "");
}

void OnMqttConnected() {
  // Here we will be spending much of our time while connected to WiFi
  MqttSubscribe();
  
  if (boolRunOnceOnWifi) // Run once
  {
    MqttPublishConnected();
    boolRunOnceOnWifi = false;
    SetOnlineWifi(WiFi.SSID().c_str());
    digitalWrite(ledPin, LOW);    // turn the LED on
  }

  for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
  {
    if (espSettings[i] != "")
    {
      CommaStrToArray(espSettings[i]);
      String setting,pin,enable,threshold,lastRun;
      for (int r = 0; r < (sizeof(arSetting) / sizeof(arSetting[0])); r++)
      {
        if(arSetting[r].indexOf("SETTING=") != -1)
          setting = arSetting[r].substring(arSetting[r].indexOf("=")+1, arSetting[r].length());
        if(arSetting[r].indexOf("PIN=") != -1)
          pin = arSetting[r].substring(arSetting[r].indexOf("=")+1, arSetting[r].length());
        if(arSetting[r].indexOf("ENABLE=") != -1)
          enable = arSetting[r].substring(arSetting[r].indexOf("=")+1, arSetting[r].length());
        if(arSetting[r].indexOf("THRESHOLD=") != -1)
          threshold = arSetting[r].substring(arSetting[r].indexOf("=")+1, arSetting[r].length());
        if(arSetting[r].indexOf("LASTRUN=") != -1)
          lastRun = arSetting[r].substring(arSetting[r].indexOf("=")+1, arSetting[r].length());
      }
      //Serial.println(setting + ":" + pin + ":" + enable + ":" + threshold + ":" + lastRun);
      if (enable == "true")
      {
        if (threshold == "0")
        {
          if (setting == "signalhigh")
            PulseStart((atoi(pin.c_str())), true);
          else if(setting == "signallow")
            PulseStart((atoi(pin.c_str())), false);
          UpdateSetting("SETTING=" + setting + "," + "PIN=" + pin + "," + "ENABLE=" + enable + "," + "THRESHOLD=1,");
        }
        if (setting == "signalhigh")
        {
          DigitalPulse((atoi(pin.c_str())), true); //true for pir
        }
        if (setting == "signallow")
        {
          DigitalPulse((atoi(pin.c_str())), false); //false for infrared
        }
        if (setting = "serialread")
        {
          //SETTING=serialread,12,13,
          CommaStrToArray(setting); // return arSetting global 
          Out("serialwrite", arSetting[1] + " : " + arSetting[2] + " : " + arSetting[3]);
          SoftwareSerial ser(atoi(arSetting[1].c_str()), atoi(arSetting[2].c_str()));
          ser.begin(115200);
          if(ser.available())
          {
            int inByte = ser.read();
            // TODO
          }
        }
        // Run on schedule 
        if ((atoi(threshold.c_str()) >= 100) && (millis() - atoi(lastRun.c_str())) > atoi(threshold.c_str()))
        {
          if (setting == "analog")
          {
            MqttPublishAnalogPin((atoi(pin.c_str())));
          }
          if (setting == "digital")
          {
            MqttPublishDigitalPin((atoi(pin.c_str())));
          }
          if (setting == "dht11")
          {
            MqttPublishDht(11,(atoi(pin.c_str())));
          }
          if (setting == "dht22")
          {
            MqttPublishDht(22,(atoi(pin.c_str())));
          }
          //UpdateSetting("SETTING=" + setting + "," + "PIN=" + pin + "," + "ENABLE=" + enable + "," + "THRESHOLD=" + threshold + "," + "LASTRUN=" + millis() + ",");
          UpdateSetting(espSettings[i].substring(0, espSettings[i].indexOf("LASTRUN=")) + "LASTRUN=" + millis() + ",");
        }
      }
    }
    else
    {
      break; //no more settings
    }
  }
}

bool MqttConnect() {
  int8_t ret;
  if (mqtt.connected()) {
    return true;
  }

  Out("MqttConnect", "Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Out("MqttConnect", ("Retrying MQTT connection in 2 seconds..."));
    mqtt.disconnect();
    delay(2000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      Out("MqttConnect", "MQTT Not connected!");
      return false;
    }
  }
  Out("MqttConnect", "MQTT Connected!");
  return true;
}

void MqttPublishAnalogPin(int pinNumber) {
  String mqttPublishStr = ("sensornet/" + (String)(chipID) + ("/analog/" + (String)pinNumber));
  pinMode(pinNumber, INPUT);
  int sensorValue = analogRead(pinNumber);
  if (sensorValue != 0)
  {
    MqttPublish(mqttPublishStr, (String)sensorValue);
  }
}

void MqttPublishDigitalPin(int pinNumber) {
  String mqttPublishStr = ("sensornet/" + (String)(chipID) + ("/digital/" + (String)pinNumber));
  pinMode(pinNumber, INPUT);
  int sensorValue = digitalRead((uint8_t)pinNumber);
  MqttPublish(mqttPublishStr, (String)sensorValue);
}

double Fahrenheit(double celsius) {
  return 1.8 * celsius + 32;
}

void MqttPublishDht(int dhtType, int pinNumber) {
  DHT_Unified dht(pinNumber, dhtType);
  dht.begin();

  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  delay(3);
  sensors_event_t eventh;  
  dht.humidity().getEvent(&eventh);

  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else
  {
    String mqttPublishStr = ("sensornet/" + (String)(chipID) + ("/dht/temperature"));
    String mqttPublishStrH = ("sensornet/" + (String)(chipID) + ("/dht/humidity"));
    MqttPublish(mqttPublishStr, (String)Fahrenheit(event.temperature));
    MqttPublish(mqttPublishStrH, (String)eventh.relative_humidity);
  }
}

void MqttPublishConnected() {
  String mqttPubStrArray[10] = {"info/ssid", "info/localip", "info/gatewayip", "info/rssi", "info/publicip", "info/version"};

  for (int i = 0; i < (sizeof( mqttPubStrArray) / sizeof(mqttPubStrArray[0])); i++)
  {
    mqttPubStrArray[i] = ("sensornet/" + (String)(chipID) + "/" + mqttPubStrArray[i]);
  }

  for (int i = 0; i < (sizeof( mqttPubStrArray) / sizeof(mqttPubStrArray[0])); i++)
  {
    switch (i)
    {
      case 0:
        {
          MqttPublish(mqttPubStrArray[i], WiFi.SSID());
          break;
        }
      case 1:
        {
          char buf[17];
          IPAddress ip = WiFi.localIP();
          sprintf(buf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
          MqttPublish(mqttPubStrArray[i], buf);
          break;
        }
      case 2:
        {
          char buf[17];
          IPAddress ipg = WiFi.gatewayIP();
          sprintf(buf, "%d.%d.%d.%d", ipg[0], ipg[1], ipg[2], ipg[3]);
          MqttPublish(mqttPubStrArray[i], buf);
          break;
        }
      case 3:
        {
          MqttPublish((String)mqttPubStrArray[i], (String)WiFi.RSSI());
          break;
        }
      case 4:
        {
          MqttPublish((String)mqttPubStrArray[i], GetPublicIP(COMMAND_HOST));
          break;
        }
      case 5:
        {
          MqttPublish((String)mqttPubStrArray[i], (String)ROM_VERSION);
          break;
        }
        DoLove();
    }
  }
}

void MqttPublish(String topic, String message) {
  Blink(2,100);
  Out("MqttPublish", (String)topic + " : " + message);
  Adafruit_MQTT_Publish mqp = Adafruit_MQTT_Publish(&mqtt, topic.c_str());
  mqp.publish(message.c_str());
}

void MqttSubscribe() {
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(250))) {
    if (subscription == &subscribeControl) {
      MqttReceived((char *)subscribeControl.lastread);
    }
    if (subscription == &subscribeM2M) {
      MqttReceived((char *)subscribeM2M.lastread);
    }
  }
}

void MqttReceived(char * myReceivedMqtt) {
  Out("MqttReceived", (String)myReceivedMqtt);

  UpdateSetting((String)myReceivedMqtt);
}

// ==================== //
// SCAN/Connect WIFI
// ==================== //
String GetNearbySSID() {
  int lowRSSI = -95;
  String nearbyAP;
  int n = WiFi.scanNetworks();

  if (n == 0)
    Out("GetNearbySSID", "No networks found");
  else
  {
    Out("GetNearbySSID", ((String)"Networks found : " + (String)n));
    for (int i = 0; i < n; ++i)
    {
      //ESP32 is WIFI_AUTH_OPEN, ESP8266 is ENC_TYPE_NONE
      if ((lowRSSI < WiFi.RSSI(i) && (WiFi.encryptionType(i) == ENC_TYPE_NONE) && ConfirmOfflineWifi(WiFi.SSID(i).c_str()) == false) || ConfirmOnlineWifi(WiFi.SSID(i).c_str()) == true)
      {
        lowRSSI = WiFi.RSSI(i);
        nearbyAP = WiFi.SSID(i);
        Out("GetNearbySSID", (String)nearbyAP + ":" + (String)lowRSSI);
      }
    }
  }
  DoLove();
  return nearbyAP;
}

bool ConnectWifi(String ap, String pass) {
  if (ap == "")
  {
    ap = GetNearbySSID();
    if (ap == onlineWifi[0]) //TODO
      pass = homeWifiPass;
  }

  Out("ConnectWifi", ap);

  if (ap != "")
  {
    WiFi.begin(ap.c_str(), pass.c_str());

    unsigned long startTimeCW = millis();
    while ((WiFi.status() != WL_CONNECTED && millis() - startTimeCW < 15000)) {
      DoLove();
    }
    if ((WiFi.status() == WL_CONNECTED))
    {
      connectWifiCount = 0;
      Blink(2, 250);
      return true;
    }
    else
    {
      Out("ConnectWifi", "Not Connected.");
      //If failed to connect x times then set offline
      connectWifiCount += 1;
      if (connectWifiCount == 5)
      {
        if (ConfirmOnlineWifi(WiFi.SSID().c_str()) == false)
        {
          SetOfflineWifi(WiFi.SSID().c_str());
        }
      }
    }
  }
  return false;
}

// Check if Wifi SSID exists in offlinewifi array
bool ConfirmOfflineWifi(const char* strWifi) {
  bool isWifiOffline = false;
  for (int i = 0; i < (sizeof(offlineWifi) / sizeof(offlineWifi[0])); ++i)
  {
    if ((strcmp(offlineWifi[i], strWifi) == 0)) // if WIFI.SSID matches offlineWifi
    {
      isWifiOffline = true;
      i = 100;
      break;
    }
  }
  return isWifiOffline;
}

//flag current wifi as offline
void SetOfflineWifi(const char * strWifi) {
  for (int i = 0; i < (sizeof(offlineWifi) / sizeof(offlineWifi[0])); ++i)
  {
    if ((strcmp(offlineWifi[i], strWifi) == 0))
    {
      i = 100; //already added, break
      break;
    }
    else if (offlineWifi[i] == "")
    {
      offlineWifi[i] = strWifi;
      Out("SetOfflineWifi", (String)strWifi);
      i = 100; //set to 100 to end for loop
      break;
    }
  }
  WiFi.disconnect();
}

bool ConfirmOnlineWifi(const char* strWifi) {
  bool isOnlineWifi = false;
  for (int i = 0; i < (sizeof(onlineWifi) / sizeof(onlineWifi[0])); ++i)
  {
    if (strcmp(onlineWifi[i], strWifi) == 0) // if WIFI.SSID matches onlineWifi
    {
      isOnlineWifi = true;
      i = 100;
      break;
    }
  }
  return isOnlineWifi;
}

void SetOnlineWifi(const char* strWifi) {
  for (int i = 0; i < (sizeof(onlineWifi) / sizeof(onlineWifi[0])); ++i)
  {
    if (strcmp(onlineWifi[i], strWifi) == 0) //check if current connected wifi matches onlineWifi list
    {
      i = 100;
      break;
    }
    else if (onlineWifi[i] == "")
    {
      onlineWifi[i] = strWifi;
      Out("SetOnlineWifi", strWifi);
      i = 100; //set to 100 to end for loop
      break;
    }
  }
}
// ROM UPDATE
String clientOutput[50];
void ConnectHttp(String host, String url, int httpPort) {
  for(int i=0; i < (sizeof(clientOutput) / sizeof(clientOutput[0])); i++) 
  {
    clientOutput[i] = "";
  }
 
  WiFiClient cl;
  if (!cl.connect(host.c_str(), httpPort)) {
    Out("ConnectHttp", "Connect Fail : Return");
    return;
  }
  // This will send the request to the server
  cl.print(String("GET ") + url + " HTTP/1.1\r\n" +
           "Host: " + host + "\r\n" +
           "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (cl.available() == 0) {
    if (millis() - timeout > 5000) {
      Out("ConnectHttp", "Connect Timeout : Return");
      cl.stop();
      return;
    }
  }
  int i;
  while (cl.available())
  {
    String line = cl.readStringUntil(' ');
    clientOutput[i] = line;
    i++;
  }
  Out("ConnectHttp", "Host: " + host + " | Url: " + url + " | Port: " + httpPort + " | Return: " + i);
}

void GetUpdate(String host) {
  bool needROMUpdate = false;
  String url = "/sensornet/update.html?espid=" + (String)chipID;
  String updateUrl;
  ConnectHttp(host, url, 80);
  for(int i = 0; i < (sizeof(clientOutput) / sizeof(clientOutput[0])); i++)
  {
    String line = clientOutput[i];
    if (line.indexOf("VER=") != -1) //Will not equal -1 if found
    {
      if (line.indexOf(ROM_VERSION) != -1)
      {
        Out("GetUpdate", "Connect Success: " + (String)line.c_str() + " = " + (String)ROM_VERSION);
      }
      else
      {
        // VER= is found but not ROM_VERSION.
        Out("GetUpdate", "Needs ROM Update: " + (String)line.c_str());
        needROMUpdate = true;
      }
    }
    if (line.indexOf("ROM=") != -1)
    {
      updateUrl = (String)line.substring((line.indexOf("ROM=") + 4), line.indexOf(".bin") + 4); //Start on line 5 to remove ROM=
    }
    if (line.indexOf("SETTING=") != -1)
    {
      // SETTINGS UPDATE
      UpdateSetting((String)line);
    }
  }
  if (needROMUpdate)
    UpdateROM(updateUrl);

  Out("GetUpdate", "Connect Success : Return");
}

// Called from GetUpdate()
void UpdateROM(String url) {
  Out("UpdateROM", "Url: " + url);
  //String url = "http://" + (String)COMMAND_HOST + "/media/" + (String)binName;
  for (int i = 0; i < 5; ++i)
  {
    Blink(4, 250);
    t_httpUpdate_return ret = ESPhttpUpdate.update(url);
    //t_httpUpdate_return  ret = ESPhttpUpdate.update("https://server/file.bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Out("UpdateROM", ("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()));
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Out("UpdateROM", ("HTTP_UPDATE_NO_UPDATES"));
        break;

      case HTTP_UPDATE_OK:
        Out("UpdateROM", ("HTTP_UPDATE_OK"));
        break;
    }
    DoLove();
  }
}

String GetPublicIP(String host) {
  String url = "/sensornet/getip.html";
  String publicIP;
  ConnectHttp(host, url, 80);
  for(int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
  {
    if (clientOutput[i].indexOf("IP=") != -1)
    {
      publicIP = (String)clientOutput[i].substring((clientOutput[i].indexOf("IP=") + 3), clientOutput[i].length());
    }
  }
  return publicIP;
}

// Handle settings string to array
// PARAM: SETTING=analog,PIN=0,THRESHOLD=10000,
// Output: arSetting[0] == SETTING=analog
//         arSetting[1] == PIN=0
void CommaStrToArray(String strSetting) {
  if(rInt == 0)
  {
    for (int r = 0; r < (sizeof(arSetting) / sizeof(arSetting[0])); r++)
    {
      arSetting[r] = "";
    }
  }
  if(strSetting.indexOf(",") != -1)
  {
    arSetting[rInt] = strSetting.substring(0, strSetting.indexOf(","));
    rInt += 1;
    CommaStrToArray(strSetting.substring(strSetting.indexOf(",")+1, strSetting.length()));
  }
  else
  {
    rInt = 0;
  }
}

// Add setting to array
void UpdateSetting(String line) {
  if (line.indexOf("SETTING=") != -1)
  {
    Out("UpdateSetting", line);
    for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
    {
      String settingName = line.substring(line.indexOf("=")+1, line.indexOf(","));
      if (espSettings[i].indexOf(settingName) != -1)
      {
        espSettings[i] = line; //update existing setting
        break;
      }
      else if (espSettings[i] == "")
      {
        espSettings[i] = line; //add new setting
        break;
      }
    }
  }
  else
  {
    if (line.indexOf("blink") != -1)
    {
      Blink(2, 250);
    }
    else if (line.indexOf("update") != -1)
    {
      GetUpdate(COMMAND_HOST);
    }
    else if (line.indexOf("publish") != -1)
    {
      MqttPublishConnected();
      for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
      {
        if (espSettings[i] != "")
        {
          UpdateSetting(espSettings[i].substring(0, espSettings[i].indexOf("LASTRUN=")) + "LASTRUN=0,");
        }
      }
    }
    else if (line.indexOf("setpin") != -1)
    {
      // ACTION=setpin,12,low,
      CommaStrToArray(line); // return arSetting global 
      Out("SetPin", arSetting[1] + " : " + arSetting[2]);
      pinMode(atoi(arSetting[1].c_str()), OUTPUT);
      if (arSetting[2] == "high")
        digitalWrite((atoi(arSetting[1].c_str())), HIGH);
      else if (arSetting[2] == "low")
        digitalWrite((atoi(arSetting[1].c_str())), LOW);
    }
    else if (line.indexOf("reset") != -1)
    {
      digitalWrite(15, LOW);
      digitalWrite(0, LOW);
      digitalWrite(2, HIGH);
      DoLove();
      ESP.restart();
    }
    else if (line.indexOf("serialwrite") != -1)
    {
      //ACTION=serialwrite,12,13,1,
      CommaStrToArray(line); // return arSetting global 
      Out("serialwrite", arSetting[1] + " : " + arSetting[2] + " : " + arSetting[3]);
      SoftwareSerial ser(atoi(arSetting[1].c_str()), atoi(arSetting[2].c_str()));
      ser.begin(115200);
      ser.write(arSetting[3].c_str());
    }
  }
}

void Blink(int count, int interval) {
  for (int i = 0; i < count; i++)
  {
    digitalWrite(ledPin, HIGH);   // turn the LED off
    delay(interval);              // wait for a second
    digitalWrite(ledPin, LOW);    // turn the LED on
    delay(interval);              // wait for a second
  }
}

void Out(String function, String message) {
  Serial.print(millis());
  Serial.print(" : ");
  Serial.print(function);
  Serial.print(" : ");
  Serial.println(message);
}

void DoLove() {
  delay(30);
}

//Digital Pulse

//the time when the sensor outputs a low impulse
long unsigned int lowTime;       
long unsigned int highTime;   
long unsigned int lowTime2;       
long unsigned int highTime2;     

//the amount of milliseconds the sensor has to be low 
//before we assume all motion has stopped
long unsigned int pause1 = 3000;  
boolean lockLow = true;
boolean takeLowTime;
boolean lockLow2 = true;
boolean takeLowTime2;

void PulseStart(int pinNumber, bool motionOnHigh) {
  Out("PulseStart", (String)pinNumber);
  
  pinMode(pinNumber, INPUT);
  if(motionOnHigh)
    digitalWrite(pinNumber, LOW);
  else
    digitalWrite(pinNumber, HIGH);
}

void DigitalPulse(int pinNumber, bool motionOnHigh) {
  if(motionOnHigh)
  {
    if(digitalRead(pinNumber) == HIGH)
    {
      //pir is HIGH on and LOW off
       if(lockLow)
       {  
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow = false;     
         highTime = millis();       
         MqttPublish(("sensornet/" + (String)(chipID) + ("/signalhigh/" + (String)pinNumber)), (String)true);
         delay(50);
       }         
       takeLowTime = true;
    }
    if(digitalRead(pinNumber) == LOW)
    {
      if(takeLowTime)
      {
      lowTime = millis();          //save the time of the transition from high to LOW
      takeLowTime = false;       //make sure this is only done at the start of a LOW phase
      highTime = 0;
      }
      //if the sensor is low for more than the given pause, assume that no more motion is going to happen
      if(!lockLow && millis() - lowTime > pause1)
      {
         //makes sure this block of code is only executed again after a new motion sequence has been detected
         lockLow = true;                        
         MqttPublish(("sensornet/" + (String)(chipID) + ( "/signalhigh/" + (String)pinNumber)), (String)false);
         delay(50);
      }
    }
  }
  else
  {
    //infrared is HIGH off and LOW on
    if(digitalRead(pinNumber) == LOW)
    {
      //pir is HIGH on and LOW off
       if(lockLow2)
       {  
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow2 = false;     
         highTime2 = millis();       
         MqttPublish(("sensornet/" + (String)(chipID) + ( "/signallow/" + (String)pinNumber)), (String)false);
         delay(50);
       }         
       takeLowTime2 = true;
    }
    if(digitalRead(pinNumber) == HIGH)
    {
      if(takeLowTime2)
      {
        lowTime2 = millis();          //save the time of the transition from high to LOW
        takeLowTime2 = false;       //make sure this is only done at the start of a LOW phase
        highTime2 = 0;
      }
      //if the sensor is low for more than the given pause, assume that no more motion is going to happen
      if(!lockLow2 && millis() - lowTime2 > pause1)
      {
         //makes sure this block of code is only executed again after a new motion sequence has been detected
         lockLow2 = true;                        
         MqttPublish(("sensornet/" + (String)(chipID) + ( "/signallow/" + (String)pinNumber)), (String)true);
         delay(50);
      }
    }
  }
}

