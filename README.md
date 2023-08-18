# IoT_ESP8266
Just another ESP8266 IOT sensor node

1. Connects to wifi
2. Gets settings from either initial hardcoded, serial input, internal web site input, or internet HTTP server
3. Run the settings on the given inverval
4. If no wifi then become access point for local control
   
Initial Settings:
@update,enable=1,time=300000,lastRun=0;
@serialRead,enable=1;
@debug,enable=1,true;

Supported Settings
@analogRead,enable=1,pin=A0,time=30000,lastRun=0;
@digitalRead,enable=1,pin=12,time=30000,lastRun=0;
@digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;
@AccessPoint,enable=1,be=true; 
@update,enable=1,time=300000,lastRun=0; //get update from http server
@loop,enable=1,time=1000; // this controls interval between RunSettings in loop()
@serialRead,enable=1; 
@debug,enable=1,be=true;
