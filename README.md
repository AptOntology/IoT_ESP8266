# IoT_ESP8266
## Just another ESP8266 IOT sensor node

1. Connects to wifi
2. Gets settings from either initial hardcoded, serial input, internal web site input, or internet HTTP server
3. Run the settings on the given inverval
4. If no wifi then become access point for local control

### Settings

#### Analog input to digital output
- Supports on/off in states overHigh, underLow, and between the analog highVal and lowVal
  
- Example (On between, off below minimum) Get input from analog pin in A0 every 10000 millis and when analog value is between 1024 and 1000 then set digital pin out 12 to high, else set to low when < 1000
-         @adCommand,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underLow,time=10000,lastRun=0;

- Example (On over highVal, off under lowVal) : Get input from analog pin, when analog value is above 1000 then set digital pin out 12 to high, if below 500 then set pin 12 to low
-         @adCommand,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1000,lowVal=500,isOn=overHigh,isOff=underLow,time=10000,lastRun=0;

- Example (On below minimum, off above maximum) Get input from analog pin, when analog value is below 1000 then set digital pin out 12 to high, if above 1024 then set pin 12 to low.
-         @adCommand,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=underLow,isOff=aboveHigh,time=10000,lastRun=0;

#### Digital input to digital output
- Example (trigger): Get input from digital pin 10 every 10000 millis and when digital read is 1(high) then set digital pin 12 to 1(high) and when digital read is 0 do nothing(isOff=no)
-         @adCommand,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=no,time=10000,lastRun=0;

- Example (flopflip): Get input from digital pin 10 every 1000 millis and when digital read is 1(high) then set digital pin 12 to 0(low) and when the digial read is 0(low) then set digital pin 12 to 1(high)
-         @adCommand,input=digital,pinIn=10,pinOut=12,isOn=low,isOff=high,time=1000,lastRun=0;

- Example (On then On, Off then off): Get input from digital pin 10 every 1000 millis and when digital read is 1(high) then set digital pin 12 to 1(high) and when the digial read is 0(low) then set digital pin 12 to 0(low)
-         @adCommand,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=low,time=1000,lastRun=0;

#### analogRead send data to server on interval 
-         @analogRead,enable=1,pin=A0,time=30000,lastRun=0;

#### digitalRead send data to server on interval 
-         @digitalRead,enable=1,pin=12,time=30000,lastRun=0;

#### digitalWrite set a pin to HIGH or LOW 
-         @digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;

#### Update the settings from the server on interval
-         @update,enable=1,time=300000,lastRun=0;

#### Initial setting: 
- System command enable update on interval, serialRead, internal http server, set loop interval to 1000
-         @system


