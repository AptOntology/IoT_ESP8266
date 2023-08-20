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
analog input to digital output, supports isOn/isOff in states overHigh,underLow, and between the highVal and lowVal
Example (On over highVal, off under lowVal) : @adCommand,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1000,lowVal=500,isOn=overHigh,isOff=underLow,time=10000,lastRun=0;
        -- > Get input from analog pin in A0 every 10000 millis and when analog value is above 1000 then set digital pin out 12 to high, if below 500 then set pin 12 to low

Example (On between, off when under minimum) : @adCommand,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underLow,time=10000,lastRun=0;
        -- > Get input from analog pin in A0 every 10000 millis and when analog value is between 1024 and 1000 then set digital pin out 12 to high, else set to low when < 1000

digital input to digital output:
Example (trigger): @adCommand,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=no,time=10000,lastRun=0;
        -- > Get input from digital pin 10 every 10000 millis and when digital read is 1(high) then set digital pin 12 to 1(high) and when digital read is 0 do nothing(isOff=no)

Example (flopflip): @adCommand,input=digital,pinIn=10,pinOut=12,isOn=low,isOff=high,time=1000,lastRun=0;
        -- > Get input from digital pin 10 every 1000 millis and when digital read is 1(high) then set digital pin 12 to 0(low) and when the digial read is 0(low) then set digital pin 12 to 1(high)

@analogRead,enable=1,pin=A0,time=30000,lastRun=0;
@digitalRead,enable=1,pin=12,time=30000,lastRun=0;
@digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;
@update,enable=1,time=300000,lastRun=0; //get update from http server

