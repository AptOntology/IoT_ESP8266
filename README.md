# IoT_ESP8266
Just another ESP8266 IOT sensor node

1. Connects to wifi / Become Access Point
2. Get unique settings from either initial, serial input, internal web server input, or internet HTTP /control.php?chipid=ESPID
3. Run the settings on the given inverval

Initial Settings:
@update,enable=1,time=30000,lastRun=0;
@serialRead,enable=1;
@debug,enable=1;
