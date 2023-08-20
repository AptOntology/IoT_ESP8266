/**
ESP8266 Client

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

Program settings:
@AccessPoint,enable=1,be=true; 
@loop,enable=1,time=1000; // this controls interval between RunSettings in loop(), a safety catch?
@serialRead,enable=1; 
@debug,enable=1,be=true;

Todo 
support for multiple @analogRead, @digitalRead per pin, currently allows 1, this will involve selecting the pin in UpdateSettings(), perhaps for each @setting with pin= 

more automatic command&control behavior patterns

backend server to dish out unique command per assigned individual or group of ESP nodes : control.php
evaluate web services to support comms, now using a php script saving inbound data via GET arguments to a flat text file while working towards sql db : data.php
result storage and html graphs

modulation and demodulation
authentication and authorization

Native libraries 
V 0.3

*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "certs.h"

#ifndef STASSID
#define STASSID "YourWifiSSID"
#define STAPSK "YourWifiPassword"
#endif

// Objects
ESP8266WiFiMulti WiFiMulti;

// Function declarations
void initSettings(); // Adds initial settings to espSettings array
String GetSettingValue(String toRun, String name); //Called from RunCommands, Takes in a @setting and name and returns the value
String GetHttpResponse(String url); //returns string payload from http
bool GetSetupConfig(); // Called from @update command in RunSettings
void UpdateSettings(String line); // Updates espSettings array when settings run or get setup
void SendData(String command, String data); // Sends data to http service and adds to lastFiveSent array
void RunCommands(); // Main 
void SetupAP(bool beAP); // Setup / Tear down an Access Point 
void Out(String function, String message); // Write serial output

// Function declarations external files
void LoophttpSrv(); // httpSrv.ino 

//Global variables
const String & hostUrl = "https://www.YourServerUrl.net/esp"; // 

String lastFiveSent[2][5]; // [1][x]=command,[0][x]=result, [2][x]=millis()?
int loopInterval = 1000; // delay between action
bool isDebugOut = true; //Enable verbose debug logging
bool autonomous; 
bool apInitialized = false;
bool isSettingsInit; //initSettings()
int sendDataCount=0; //SendData()
String theStatus; // system status for internal web server
String espSettings[20];

//Functions
void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("On!");
  Out("setup","Connecting to ssid...'" STASSID "'");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(STASSID, STAPSK);
}

void loop() {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    RunCommands();
  }
  else
  {
    // No WIfi? be autonomous AP for a while 
    // Still need RunCommands(noWIFI==true) for serialRead,debug,access point loops
    isDebugOut = autonomous = true; 
    SetupAP(true);
    theStatus = "No Wifi!";
    RunCommands();
  }
  LoophttpSrv();
  delay(loopInterval);
}

void initSettings()
{
  espSettings[0] = "@update,enable=1,time=300000,lastRun=0;"; 
  espSettings[1] = "@serialRead,enable=1;"; 
  espSettings[2] = "@debug,enable=1,be=false;";
  theStatus = "Initialized.";
  lastFiveSent[0][0] = "";
  Out("debug initSettings IP", (String)WiFi.localIP().toString());
  isSettingsInit = true;
}

String GetSettingValue(String toRun, String name) 
{ 
  String thingStr1 = toRun.substring(toRun.indexOf(name),toRun.indexOf(";")); // pin=0,time=10,lastRun=0; // could end in , or ;
  String thingStr;
  if(thingStr1.indexOf(",") > thingStr1.indexOf(";"))
  {
    thingStr = thingStr1.substring(thingStr1.indexOf("=")+1,thingStr1.indexOf(",")); // 0
  }
  else
  {
    thingStr = thingStr1.substring(thingStr1.indexOf("=")+1,thingStr1.indexOf(";")); // 0
  }
  return thingStr; 
}

String GetHttpResponse(String url) {
  Out("debug GetHttpResponse url", url);
  String payload;
     std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint_sni_cloudflaressl_com);
    client->setInsecure();

    HTTPClient https;
    //Out("loop","[HTTPS] begin...\n");
    if (https.begin(*client, url)) {  // HTTPS
      int httpCode = https.GET();
      Out("debug GetHttpResponse httpCode 1: ", (String)httpCode);

      // httpCode will be negative on error
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          payload = https.getString();
          Out("debug GetHttpResponse payload", payload);
        }
      }
    }
    else
    {
      Out("debug GetHttpResponse https.begin ", "false");
    }
    return payload;
}

bool GetSetupConfig() {
  bool toReturn = false; // returns true if @update is a setting returned from the server
  String chipID = (String)ESP.getChipId();
  String controlUrl = hostUrl + "/control.php/?chipid="+chipID;
  String payload = GetHttpResponse(controlUrl);
  Out("debug GetSetupConfig url", controlUrl);
  Out("debug GetSetupConfig payload", payload);
  int loopLength = 0;
  int atIndex = 0;
  int semiIndex = 0;
  String subPayload = payload;
  String payloadTemp = payload;  //calculate amount of lines in payload, add up the semicolon
  payloadTemp.replace(";","");
  int payloadLength = payload.length()-payloadTemp.length();

  Out("debug GetSetupConfig payloadLength", (String)payloadLength);

  while(loopLength <= payloadLength)
  {
    if(semiIndex == 0)  //first entry
    {
      atIndex = subPayload.indexOf("@");
      semiIndex = subPayload.indexOf(";");
    }
    else
    {
      //from the second to the last entry, subString the payload ( This removes the previous line from the payload)
      subPayload = subPayload.substring(semiIndex+1,(subPayload.length())); // add one to exclude the last ;
      atIndex = subPayload.indexOf("@");
      semiIndex = subPayload.indexOf(";");
    }
    // at index should always be 1, semiIndex should always be the length of the string, the last entry may or may not have a ; (lol)
    String theCmd = subPayload.substring(atIndex,semiIndex+1);

    if(theCmd.indexOf("@update") != -1)  // if the command is update then the server is updating @update. Used in RunSettings @update to check if @update needs UpdateSettings for LastRun
    {
      toReturn = true; 
    }
    if(theCmd.indexOf("@") == 0) // every setting must start with @, if >= or otherwise then its broken setting, could also check for trailing ; while we're here
    {
      UpdateSettings(theCmd);
      theStatus = "Server conf!";
    }
    else
    {
      Out("debug GetSetupConfig invalid setting format", theCmd);
    }

    if(semiIndex == 0 || semiIndex == -1)
    {
      loopLength = payload.length(); // last one
    }
    loopLength++;
  }
  return toReturn; // false if server didn't return @update string
}

void UpdateSettings(String line)
{
  if(line != "")
  {
    for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
    {
      String settingName = line.substring(line.indexOf("@")+1, line.indexOf(","));
      if (espSettings[i].indexOf(settingName) != -1)
      {
        Out("debug UpdateSettings update ", line);
        // Three states:0,1,2 : update to incoming lastrun = 0, update setting and keep previous last run, update setting and set lastrun to millis() 
        // if incoming lastRun = 0 then set to 0, update lastRun cycle with millis if greater than 10, and set to previous lastRun if between 1-10 (This opens up some scenariotastic intervals)
        // The server sends @update command with lastRUn value greater than 10 and then UpdateSettings sets the last run to the current run time
        if(line.indexOf("lastRun") > 0)
        {
          String incomingLastRun = GetSettingValue(line,"lastRun");
          if(incomingLastRun == "0") // update as-is, runs on next interval
          {
            Out("debug UpdateSettings incomingLastRun ", (String)incomingLastRun);
            espSettings[i] = line; //update existing setting
          }
          else if(atoi(incomingLastRun.c_str()) < 10) // if less than 10 then set to the previous run as though the command didnt run, this is useful to update the command setting without changing the actual run interval
          {
            String previousRun = GetSettingValue(espSettings[i],"lastRun");
            espSettings[i] = line.substring(0,line.indexOf("lastRun=")) + "lastRun="+previousRun+";";
          }
          else //else update LastRun with the current millis // incoming last run greater than 10 indicates we want to change it to now as it just ran or otherwise
          {
            //Out("debug UpdateSettings incomingLastRun > 10", (String)incomingLastRun);
            long theTime = millis();
            espSettings[i] = line.substring(0,line.indexOf("lastRun="))+"lastRun="+theTime+";";  
          }
        }
        else
        {
          Out("debug UpdateSettings update with no LastRun : ", ((String)line + " | setting # : " + (String)i));
          espSettings[i] = line; //update existing setting
        }
        break;
      }
      else if (espSettings[i] == "")
      {
        Out("debug UpdateSettings add ", line);
        espSettings[i] = line; //add new setting
        break;
      }
    }
  }
  else
  {
    Out("debug UpdateSettings Line enmpty ", line);
  }
}

void SendData(String command, String data)
{
    Out("debug SendData command",command);
    Out("debug SendData data", data);
    String dataUrl = hostUrl + "/data.php";
    String chipID = (String)ESP.getChipId();
    String response = GetHttpResponse((dataUrl)+"?chipid="+chipID+"&cmd="+command+"&data="+data);
    Out("debug SendData response", response);
    lastFiveSent[0][sendDataCount] = command;
    lastFiveSent[1][sendDataCount] = data;
    //lastFiveSent[2][sendDataCount] = (String)millis(); // todo this causes a reset?
    if(sendDataCount == 4) { sendDataCount = 0; } else { sendDataCount += 1;};
}

void RunCommands() 
{
  if(isSettingsInit == false) { initSettings(); }  // if settings empty then initialize
  for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
  {
    if (espSettings[i] != "")
    {
      String toRun = espSettings[i];
      String enableStr = GetSettingValue(toRun,"enable");
      if(enableStr == "1" || enableStr != "0")
      {
        Out("debug RunCommands enabled ",toRun);
        if(toRun.startsWith("@analogRead")) //@analogRead,enable=1,pin=A0,returnMaxMin=1,time=10000,lastRun=0;
        {
          String pinStr = GetSettingValue(toRun,"pin");
          String intervalStr = GetSettingValue(toRun,"time"); 
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          String returnMaxMinStr = GetSettingValue(toRun,"returnMaxMin");
          if(pinStr.indexOf("A") != -1 && pinStr.length() <= 3) // this must start with A and must be less than 3
          {
            if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))  // Get time interval to run and last run time. If time to run then run
            {
              String analogValue = (String)analogRead(atoi(pinStr.c_str()));
              Out("RunCommands",((String)"analogRead " + (String)pinStr + " value : " + analogValue));
              if(analogValue != "1024" && analogValue != "0")
              {
                SendData("analogRead",analogValue);
              } 
              else if(returnMaxMinStr == "1")
              {
                SendData("analogRead",analogValue);
              }
              UpdateSettings("@analogRead,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");
            }
          }
          else
          {
            Out("debug RunCommands analogRead pinStr invalid : ", pinStr);
          }
        }
        if(toRun.startsWith("@digitalRead")) // toRun : @digitalRead,pin=0,time=10000,lastrun=12345; 
        {
          String pinStr = GetSettingValue(toRun,"pin");
          String intervalStr = GetSettingValue(toRun,"time"); 
          String lastRunStr = GetSettingValue(toRun,"lastRun");

          if(pinStr.length() >= 1 && pinStr.length() <= 1) // this must be =>1 and <=2
          {
            if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))  // Get time interval to run and last run time. If time to run then run
            {
              pinMode(atoi(pinStr.c_str()), INPUT);
              String digitalValue = (String)digitalRead(atoi(pinStr.c_str()));
              Out("RunCommands",((String)"digitalRead " + (String)pinStr + " value : " + digitalValue));
              SendData("digitalRead",digitalValue);
              UpdateSettings("@digitalRead,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");
            }
          }
          else
          {
            Out("debug RunCommands digitalRead pinStr invalid : ", pinStr);
          }
        }
        if(toRun.startsWith("@digitalWrite"))   // toRun : @digitalWrite,enable=1,pin=0,type=HIGH,time=1000,lastrun=12345;
        {
          String pinStr = GetSettingValue(toRun,"pin");
          String intervalStr = GetSettingValue(toRun,"time"); 
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          String typeStr = GetSettingValue(toRun,"type");

          if(pinStr.length() >= 1 && pinStr.length() <= 1) // pin must be =>1 and <=2
          {
            if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))  // Get time interval to run and last run time. If time to run then run
            {
              pinMode(atoi(pinStr.c_str()), OUTPUT);
              if(typeStr == "HIGH")
              {
                digitalWrite(atoi(pinStr.c_str()),HIGH);
              }
              else if(typeStr == "LOW")
              {
                digitalWrite(atoi(pinStr.c_str()),LOW);
              }
              Out("RunCommands",((String)"digitalWrite " + (String)pinStr + " " + (String)typeStr));
            }
          }
          else
          {
            Out("debug RunCommands digitalWrite pinStr invalid : ", pinStr);
          }
        } 
        if(toRun.startsWith("@adCommand")) 
        {      
          // @adCommand,input=analog,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underLow,time=10000,lastRun=0;
          // -- > Get input from analog pin A0 every 10000 millis and when the analog value is between 1024 and 1000 then set digital pin 12 to high, else set to low when < 1000
          /* 
          isOn / isOff states
          isOn=overHigh
          isOn=underLow
          isOn=between
          isOn=no
          isOff=overHigh
          isOff=underLow
          isOff=between
          isOff=no
          */      
          String inStr = GetSettingValue(toRun,"input");
          String pinInStr = GetSettingValue(toRun,"pinIn");
          String pinOutStr = GetSettingValue(toRun,"inOut");
          String hiValStr = GetSettingValue(toRun,"highVal");
          String loValStr = GetSettingValue(toRun,"lowVal");
          String isOnStr = GetSettingValue(toRun,"time");
          String isOffStr = GetSettingValue(toRun,"time");
          String intervalStr = GetSettingValue(toRun,"time");
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
          {
            Out("debug RunCommandsupdate adCommand", "Running");
            if(inStr == "analog")
            {
              int analogValue = analogRead(atoi(pinInStr.c_str())); // 0 to 1024
              bool isAnalogBelowHigh = (analogValue < atoi(hiValStr.c_str())); 
              bool isAnalogAboveLow = (analogValue > atoi(loValStr.c_str())); 
              Out("RunCommands adCommand analog",((String)analogValue + " : " + (String)isAnalogBelowHigh  + " " + (String)isAnalogAboveLow));
              //Tri state on off
              if(isOnStr == "overHigh")
              {
                if(!(isAnalogBelowHigh)) 
                {
                  // do it
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                   Out("debug adCommand setting pin high", pinOutStr);
                }
              } 
              else if (isOnStr == "underLow")
              {
                if(!(isAnalogAboveLow))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                   Out("debug adCommand setting pin high", pinOutStr);
                }
              }
              else if (isOnStr == "between")
              {
                if(isAnalogBelowHigh && isAnalogAboveLow)
                {
                  // do it
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                  Out("debug adCommand setting pin high", pinOutStr);
                }
              }

              // now turn off
              if(isOffStr == "overHigh")
              {
                if(!(isAnalogBelowHigh))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                   Out("debug adCommand setting pin low", pinOutStr);
                }
              }
              else if (isOffStr == "underLow")
              {
                if(!(isAnalogAboveLow))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                   Out("debug adCommand setting pin low", pinOutStr);
                }
              }
              else if (isOnStr == "between")
              {
                if(isAnalogBelowHigh && isAnalogAboveLow)
                {
                  // do it
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                   Out("debug adCommand setting pin low", pinOutStr);
                }
              }
            }
            else if(inStr == "digital")
            {
              //digital read
              pinMode(atoi(pinInStr.c_str()), INPUT);
              bool digitalValue = digitalRead(atoi(pinInStr.c_str()));
              Out("RunCommands adCommand digital",((String)digitalValue));

              if(isOnStr == "high")
              {
                if(digitalValue == 1)
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                }
              }
              else if (isOnStr == "low")
              {
                if(digitalValue == 0)
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                }
              }
            }
            UpdateSettings(espSettings[i].substring(0,espSettings[i].indexOf("lastRun="))+"lastRun="+(String)millis()+";");
          }
        }
        if(toRun.startsWith("@sendStatus"))   // @SendStatus,enable=1,net=1,sys=1,time=10000,lastRun=0;
        {
          // Internal and Public IP ? 
          // DNS query
          String netStr = GetSettingValue(toRun,"net");
          String sysStr = GetSettingValue(toRun,"sys"); // all system status bundled
          if(netStr == "1")
          {
            SendData("localIP", (String)WiFi.localIP().toString());
            SendData("gatewayIP", (String)WiFi.gatewayIP().toString());;
            Out("debug RunCommands SendStatus", (String)"net");
            UpdateSettings("@sendStatus,enable=1,net=0;");
          }
          if(sysStr == "1")
          {
            SendData("uptime", String(millis() / 60000));
            SendData("status", theStatus);
            Out("debug RunCommands SendStatus", (String)"sys");
            UpdateSettings("@sendStatus,enable=1,net=0;");
          }          
        }
        if(toRun.startsWith("@accessPoint")) //@accessPoint,enable=1,be=true; // does this need an interval or can we flipflop the boolean?
        {
          String beStr = GetSettingValue(toRun,"be");
          if(beStr == "true")
          {
              SetupAP(true);
              //Out("debug RunSettings AccessPoint", "true");
          }
          else
          {
            SetupAP(false);         
            //Out("debug RunSettings AccessPoint", "false");
          }
        }
        if(toRun.startsWith("@loop")) //@loop,enable=1,time=1000;
        {
          //String intervalStr = GetSettingValue(toRun,"time");
          long newLoopInterval = atoi(GetSettingValue(toRun,"time").c_str());
          if(loopInterval != newLoopInterval)
          {
            loopInterval = newLoopInterval;
            Out("debug RunSettings loop set ", (String)newLoopInterval);
          }
        }
        if(toRun.startsWith("@update")) //@update,enable=1,time=10000,lastRun=0;
        {            
          String intervalStr = GetSettingValue(toRun,"time");
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
          {
            Out("debug RunCommandsupdate ", "Running GetSetupConfig()");
            // This is the only time where GetSetupConfig gets run it returns true when @update is included in the setup??? needs todo
            // Perhaps change the name to GetUpdate
            if(GetSetupConfig())
            {
              Out("RunCommands GetSetupConfig returned", "true");
              // The server sends @update command with lastRUn value greater than 10 and then UpdateSettings sets the last run to the current run time
            }
            else
            {
              // no @update from server, update the LastRun to now and keep the current intervalStr
              if(intervalStr == "") { intervalStr = "45000";} // if no interval then set to 45 seconds
              UpdateSettings("@update,enable=1,time="+intervalStr+",lastRun="+millis()+";");              
            }
          }
        }
        if(toRun.startsWith("@debug"))  //@debug,enable=1,be=true;
        {
          String beStr = GetSettingValue(toRun,"be");
          if(beStr == "false")
          {
            isDebugOut = false;
          }
          else if(beStr == "true")
          {
            isDebugOut = true;
          }
        }
        if(toRun.startsWith("@serialRead")) //@serialRead,enable=1;
        {
          if(Serial.available())
          {
            Out("RunCommands","Reading serial...");
            String readStr = "";
            readStr = Serial.readString();
            delay(2);
            if(readStr != "")
            {
              readStr = readStr.substring(0,readStr.length()-1);
              readStr.trim();
              Out("RunCommands serialRead",readStr);
              if(readStr.startsWith("@"))
              {
                theStatus = "serial control!";
                UpdateSettings(readStr);
              }
              else
              {
                SendData("serialRead",readStr);
              }
            }
          }
        }
      }
    }
  }
}

void SetupAP(bool beAP) {
  if(!apInitialized)
  {
    pinMode(A0, INPUT);
    apInitialized = true;
    if(beAP)
    {
      WiFi.mode(WIFI_AP_STA);
      WiFi.disconnect();
      delay(50);
      WiFi.softAP(("ESP_" + (String)(ESP.getChipId())).c_str(), "");
    }
    else
    {
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(50);
      apInitialized = false;
    }
    Out("SetupAP", "Complete");
  }
}

void Out(String function, String message) {
  bool isOut;
  if(function.startsWith("debug") && isDebugOut) {
    isOut = true;
  }
  else if(function.startsWith("debug") && isDebugOut == false)
  {
    isOut = false;
  }
  else
  {
    isOut = true;
  }
  if(isOut)
  {
    Serial.print(millis());
    Serial.print(" : ");
    Serial.print(function);
    Serial.print(" : ");
    Serial.println(message);
  }
}
