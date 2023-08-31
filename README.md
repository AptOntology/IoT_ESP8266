# IoT_ESP8266
## Just another ESP8266 IOT sensor node

1. Connects to wifi
2. If no wifi then be access point
3. Initialize settings then get from either serial input, internal web server form, or HTTPS server
4. Run the settings on inverval

### Settings

#### Initial setting: 
- System command: loop delay to 1000 millis, internal web server enabled, get update from http every 200000 millis, enable serial debugger, enable serial read, lastRun=1(runOnce)
-         @system,enable=1,newLoop=1000,accessPoint=0,httpSrv=1,update=200000,debug=1,serialRead=1,time=10000,lastRun=1;

#### the lastRun switch : 
 - 0 is setting as-is and run on next interval
 - 1 is runOnce and set enable=0
 - 2 is update setting and keep the previous lastRun
 - >10 is run on interval and update lastRun to now

#### Analog input to digital output
- isOn/isOff in states overHigh, underLow, overUnder, and between the highVal and lowVal, if isOn(on) elseif isOff(off)

- Example (On between, off below minimum) Get input from analog pin in A0 every 10000 millis and when analog value is between 1024 and 1000 then set digital pin out 12 to high, else set to low when < 1000
-         @sampleExecute,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underLow,time=10000,lastRun=0;

- Example (On over highVal, off under lowVal) : Get input from analog pin, when analog value is above 1000 then set digital pin out 12 to high, if below 500 then set pin 12 to low
-         @sampleExecute,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1000,lowVal=500,isOn=overHigh,isOff=underLow,time=10000,lastRun=0;

- Example (On below minimum, off above maximum) Get input from analog pin A0 every 1000 millis, when analog value is below 100 then set digital pin out 12 to high, if above 1000 then set pin 12 to low.
-         @sampleExecute,input=analog,out=digital,pinIn=A0,pinOut=12,highVal=1000,lowVal=100,isOn=underLow,isOff=aboveHigh,time=1000,lastRun=0;

- Example (On between, off over high and under low)
-         @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=800,lowVal=400,isOn=between,isOff=overUnder,time=1000,lastRun=0;

#### Digital input to digital output
- Example (trigger): Get input from digital pin 10 every 10000 millis and when digital read is 1(high) then set digital pin 12 to 1(high) and when digital read is 0 do nothing(isOff=no)
-         @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=no,time=10000,lastRun=0;

- Example (flopflip): Get input from digital pin 10 every 1000 millis and when digital read is 1(high) then set digital pin 12 to 0(low) and when the digial read is 0(low) then set digital pin 12 to 1(high)
-         @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=low,isOff=high,time=1000,lastRun=0;

- Example (Relay): Get input from digital pin 10 every 1000 millis and when digital read is 1(high) then set digital pin 12 to 1(high) and when the digial read is 0(low) then set digital pin 12 to 0(low)
-         @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=low,time=1000,lastRun=0;

#### analogRead pin A0 every 100000 millis and send data to output, no return for 1024 or 0
-         @analogRead,enable=1,pin=A0,returnMaxMin=0,time=100000,lastRun=0;

#### digitalRead pin 12 every 30000 millis and send data to output 
-         @digitalRead,enable=1,pin=12,time=30000,lastRun=0;

#### digitalWrite set a pin to HIGH or LOW 
-         @digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;

#### Update the settings from the server on interval
-         @update,enable=1,time=300000,lastRun=0;

#### Send network and system details to output
-         @sendStatus,enable=1,net=1,sys=1,time=10000,lastRun=1; 

#### Enable network announce services
-         @network,mdns=1,ssdp=1,llmnr=1,lastRun=1;

#### Set the HTTPS host URL
-         @newServer,host=http://server.com/esp,lastRun=1;
