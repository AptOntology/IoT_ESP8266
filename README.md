# IoT_ESP8266
Just another ESP8266 IOT sensor node

1. Scans for open WiFi or connect to home WiFi
      If unable to connect to same WiFi network 3 times then ban WiFi network
    
2. Connects to HTTP server to get update and settings at
      URL: http://YourServerName/sensornet/update.html
      Update.html should return a string:
      VER=01 ROM=http://www.YourServerName/media/IoT_ESP8266.ino.bin SETTING=dht11,PIN=12,ENABLE=true,THRESHOLD=960000,LASTRUN=0, SETTING=analog,PIN=0,ENABLE=true,THRESHOLD=960000,LASTRUN=0,
  
3. Connects to MQTT server and begins loop
      If unable to connect to MQTT server then ban WiFi network and goto step 1 (exception for home wifi)
    
4. Publishes sensor values to MQTT
      Publishes on demand in the case of reading a change from digital HIGH to LOW or LOW to HIGH
      Publishes on threshold defined by SETTING then update the LASTRUN to be the last publish time
   
SETTING=analog,PIN=0,ENABLE=true,THRESHOLD=960000,LASTRUN=0,
~ Publish value of analog pin 0 every 960 seconds
  
SETTING=digital,PIN=12,ENABLE=true,THRESHOLD=960000,LASTRUN=0,
SETTING=dht11,PIN=12,ENABLE=true,THRESHOLD=960000,LASTRUN=0, 
SETTING=dht22,PIN=12,ENABLE=true,THRESHOLD=960000,LASTRUN=0, 
  
SETTING=signalhigh,PIN=12,ENABLE=true,THRESHOLD=0,
~ Publish if PIN 12 is HIGH
SETTING=signallow,PIN=12,ENABLE=true,THRESHOLD=0,
~ Publish if PIN 12 is LOW
