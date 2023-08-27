/**
ESP8266 Client

1. Connect to wifi
2. Get settings from either initial hardcoded, serial input, internal web site input, or internet HTTP server
3. Run the settings on inverval

Initial Settings:
@system,enable=1,newLoop=1000,accessPoint=0,httpSrv=1,update=200000,debug=1,serialRead=1,time=10000,lastRun=0;

Settings
# lastRun switch : 0 is runNow and update lastRun to now, 1 is runOnce and set enable=0, 2-10 is update setting and keep the previous lastRun, above 10 is to run on interval and update lastRunStrto now

analog input to digital output
isOn/isOff while overHigh, underLow, overUnder, or between the highVal or lowVal | if isOn(on) elseif isOff(off)
Example (On over highVal, off under lowVal) : @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=1000,lowVal=500,isOn=overHigh,isOff=underLow,time=10000,lastRun=0;
Example (On between, off when under minimum) : @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underLow,time=10000,lastRun=0;
Example (On between, off over high and under low) : @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=800,lowVal=400,isOn=between,isOff=overUnder,time=1000,lastRun=0;

digital input to digital output:
Example (trigger): @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=no,time=10000,lastRun=0;
Example (flopflip): @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=low,isOff=high,time=1000,lastRun=0;

@analogRead,enable=1,pin=A0,returnMaxMin=1,time=100000,lastRun=0;
@digitalRead,enable=1,pin=12,time=30000,lastRun=0;
@digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;
@SendStatus,enable=1,net=1,sys=1,time=10000,lastRun=0; //send status on interval
@SendStatus,enable=1,net=1,sys=1,time=10000,lastRun=1; //send status once
@system,enable=1,newLoop=500,accessPoint=0,httpSrv=0,update=200000,debug=0,serialRead=0,time=100000,lastRun=0; // production mode
@network,mdns=1,ssdp=1,llmnr=1,dnsHost=1,lastRun=1; // todo, dnsHost cmd to dish out the new hostURL when offline
@update,enable=1,time=300000,lastRun=0; //get update from http server
@update,enable=1,lastRun=1; //get update from http server once
@serialRead,enable=1,lastRun=1; 

Todo 
test and debug use case scenarios

mesh/standalone mode
If no initial wifi: scan for open ESP_ID### AP, connect, get settings, and run commands function as usual. 
If no ESP_ID: be ap, dns(hostURL), and http server and dish out settings through hostURL/control.php. Receive by data.php to array, Store&forward
New @receiveToSend command
- forward incoming to internet hostUrl

device support: GPIO, PWM, I2C, SPI

control.php: Get request, arg ESPID : returns string of settings 
data.php: Get request, args ESPID,command,result :  saves to server

authentication and authorization
- calculated PSK from ESPID, pseudo-secure 

Native libraries 
V 0.4
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
bool GetUpdateFromHost(); // Called from @update command in RunCommands
void UpdateSettings(String line); // Updates espSettings array when settings run or get setup
void SendData(String command, String data); // Sends chipId, command, data to http get and adds to sendDataOut[][] array
void RunCommands(); // Main 
void SetupAP(bool beAP); // Setup / Tear down an Access Point 
void Out(String function, String message); // Write serial output

// Function declarations external files
void LoophttpSrv(); // httpSrv.ino 
void setuphttpSrv();
void setupNet();
void loopNet();

//Global variables
String hostUrl = "https://www.YourServerUrl.com/esp"; // 

float programVersion = 0.01;
String sendDataOut[3][20]; // [0][x]=command,[1][x]=result, [2][x]=millis()
bool isDebugOut = true; //Enable verbose debug logging Out()
bool isSettingsInit = false; //initSettings()
bool apInitialized = false; //SetupAP()
bool behttpSrv = false; //enabled through @httpSrv setting
bool beNetSrv = false; //enabled through @network setting
int sendDataCount=0; //SendData()
int loopInterval = 1000; // delay between action
int defaultIntervalStr = 450000; //if a setting has no interval then add default
String theStatus; // short string system status for Output
String espSettings[20]; // holds commands to run on schedule or trigger

//Functions
void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("On!");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(STASSID, STAPSK);
  Out("setup","Add ssid...'" STASSID "'");
  initSettings();
}

void loop() {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    RunCommands();
  }
  else
  {
    isDebugOut = true; 
    theStatus = "No Wifi!";
    SetupAP(true);
    RunCommands();
  }
  if(behttpSrv)
  {
    LoophttpSrv();
  }
  if(beNetSrv)
  {
    loopNet();
  }
  delay(loopInterval);
}

void initSettings() {
  espSettings[0] = "@system,enable=1,newLoop=1000,accessPoint=0,httpSrv=1,update=200000,debug=1,serialRead=1,time=10000,lastRun=0;";
  theStatus = "Initialized";
  isSettingsInit = true;
}

String GetSettingValue(String toRun, String name) { 
  String cmdStr1 = toRun.substring(toRun.indexOf(name),toRun.indexOf(";")); // pin=0,time=10,lastRun=0; // could end in , or ;
  String cmdStr;
  if(cmdStr1.indexOf(",") > cmdStr1.indexOf(";"))
  {
    cmdStr = cmdStr1.substring(cmdStr1.indexOf("=")+1,cmdStr1.indexOf(","));
  }
  else
  {
    cmdStr = cmdStr1.substring(cmdStr1.indexOf("=")+1,cmdStr1.indexOf(";"));
  }
  return cmdStr; 
}

String GetHttpResponse(String url) {
  Out("debug GetHttpResponse url", url);
  String payload;
     std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint_sni_cloudflaressl_com);
    client->setInsecure();

    HTTPClient https;
    if (https.begin(*client, url)) {  // HTTPS
      int httpCode = https.GET();
      //Out("debug GetHttpResponse httpCode 1: ", (String)httpCode);
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          payload = https.getString();
          if(payload.length() > 0) { 
            Out("debug GetHttpResponse payload", payload);
          }
          else
          {
            Out("debug GetHttpResponse payload", "No payload received");
          }
        }
      }
    }
    else
    {
      Out("debug GetHttpResponse https.begin ", "false");
    }
    return payload;
}

bool GetUpdateFromHost() {
  bool toReturn = false; // returns true if @update is a setting returned from host
  String chipID = (String)ESP.getChipId();
  String controlUrl = hostUrl + "/control.php/?chipid="+chipID;
  String payload = GetHttpResponse(controlUrl);
  int loopLength = 0;
  int atIndex = 0;
  int semiIndex = 0;
  String subPayload = payload;
  String payloadTemp = payload;  //calculate amount of lines in payload, add up the semicolon
  payloadTemp.replace(";","");
  int payloadLength = payload.length()-payloadTemp.length();
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
    // at index should always be 1, semiIndex should always be the length of the string
    String theCmd = subPayload.substring(atIndex,semiIndex+1);

    if(theCmd.indexOf("@update") != -1)  // if the command is update then the server is updating @update. Used in RunCommands @update to check if @update needs UpdateSettings for LastRun
    {
      toReturn = true; 
    }
    if(theCmd.indexOf("@") == 0) // every setting must start with @, if >= or otherwise then its broken setting, could also check for trailing ; while we're here
    {
      UpdateSettings(theCmd);
      theStatus = "Server config.";
    }
    else
    {
      Out("debug GetUpdateFromHost invalid setting format", theCmd);
    }

    if(semiIndex == 0 || semiIndex == -1)
    {
      loopLength = payload.length(); // last one
    }
    loopLength++;
  }
  return toReturn; // false if server didn't return @update string
}

void UpdateSettings(String line) {
  Out("debug UpdateSettings line", line);
  if(line != "")
  {
    for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
    {
      String settingName = line.substring(line.indexOf("@")+1, line.indexOf(","));
      String pinStr = GetSettingValue(line,"pin");
      if (espSettings[i].indexOf(settingName) != -1 && (pinStr == "" || espSettings[i].indexOf("pin="+pinStr) != -1)) //if incoming pin is empty or pin matches setting then update
      {
        // Three states:0,1,2 : update to incoming lastRunStr= 0, update setting and keep previous last run, update setting and set lastRunStrto millis() 
        // if incoming lastRunStr= 0 then set to 0, update lastRunStrto now millis() if greater than 10, and set to previous lastRunStrif between 1-10 (allowing to change settings and keep next run time)
        // The server sends @update command with lastRunStrvalue greater than 10 and then UpdateSettings sets the last run to the current run time
        if(line.indexOf("lastRun") > 0)
        {
          Out("debug UpdateSettings update ", line);
          String incominglastRunStr = GetSettingValue(line,"lastRun");
          if(incominglastRunStr == "0") // update as-is, runs on next interval
          {
            espSettings[i] = line; //update existing setting
          }
          else if(incominglastRunStr == "1") // 1 is trigger, keep 1 to trigger on next run
          {
            espSettings[i] = line.substring(0,line.indexOf("lastRun=")) + "lastRun=1;";
          }
          else if(atoi(incominglastRunStr.c_str()) < 10) // if less than 10 then set to the previous run as though the command didnt run, this is useful to update the command setting without changing the actual run interval
          {
            String previousRun = GetSettingValue(espSettings[i],"lastRun");
            //todo check for previousRun to be?
            espSettings[i] = line.substring(0,line.indexOf("lastRun=")) + "lastRun="+previousRun+";";
          }
          else //else update lastRunStrwith the current millis // incoming last run greater than 10 indicates we want to change it to now as it just ran or otherwise
          {
            long theTime = millis();
            espSettings[i] = line.substring(0,line.indexOf("lastRun="))+"lastRun="+theTime+";";  
          }
        }
        else
        {
          Out("debug UpdateSettings update with no lastRunStr: ", ((String)line + " | setting # : " + (String)i));
          espSettings[i] = line; //update existing setting
        }
        break;
      }
      else if (espSettings[i] == "") // todo , run through the whole loop first then add
      {
        Out("debug UpdateSettings add ", line);
        espSettings[i] = line; //add new setting
        break;
      }
    }
  }
  else
  {
    Out("debug UpdateSettings Line empty ", line);
  }
}

void SendData(String command, String data) {
    Out("debug SendData command",command);
    Out("debug SendData data", data);
    String dataUrl = hostUrl + "/data.php";
    String chipID = (String)ESP.getChipId();
    String response = GetHttpResponse((dataUrl)+"?chipid="+chipID+"&cmd="+command+"&data="+data);
    Out("debug SendData response", response);
    sendDataOut[0][sendDataCount] = command;
    sendDataOut[1][sendDataCount] = data;
    sendDataOut[2][sendDataCount] = (String)(millis()/60000);
    if(sendDataCount == 4) { sendDataCount = 0; } else { sendDataCount += 1;};
}

void RunCommands() {
  if(!(isSettingsInit)) { initSettings(); }  // if settings empty then initialize
  for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
  {
    if (espSettings[i] != "")
    {
      String toRun = espSettings[i];
      String enableStr = GetSettingValue(toRun,"enable"); 
      if(enableStr == "1" || enableStr != "0")
      {
        // todo, single lastRunStrcheck for each interval setting
       // todo set lastRun=1 to equal a trigger(enable=0) 

        String intervalStr = GetSettingValue(toRun,"time"); 
        String lastRunStr = GetSettingValue(toRun,"lastRun");
        if(intervalStr == "") { intervalStr = defaultIntervalStr;} // if no interval then set to 450 seconds

        if(lastRunStr == "1" || lastRunStr == "0" || millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str())) //if lastRun=0 OR interval match OR trigger then run NOW
        {
          Out("debug RunCommands to run ",toRun);

          if(toRun.startsWith("@analogRead")) //@analogRead,enable=1,pin=A0,returnMaxMin=1,time=10000,lastRun=0;
          {
            String pinStr = GetSettingValue(toRun,"pin");
            String returnMaxMinStr = GetSettingValue(toRun,"returnMaxMin");
            
            if(pinStr.indexOf("A") != -1 && pinStr.length() <= 3) // this must start with A and must be less than 3
            {
              if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))  // Get time interval to run and last run time. If time to run then run
              {}
                String analogValue = (String)analogRead(atoi(pinStr.c_str()));
                Out("RunCommands",((String)"analogRead " + (String)pinStr + " maxMin : " + returnMaxMinStr + " value : " + analogValue));
                if(analogValue != "1024" && analogValue != "0")
                {
                  SendData("analogRead "+pinStr,analogValue);
                } 
                else if(returnMaxMinStr == "1")
                {
                  SendData("analogRead "+pinStr,analogValue);
                }
                if(lastRunStr == "1")
                {
                  UpdateSettings("@analogRead,enable=0,pin="+pinStr+",returnMaxMin=" + returnMaxMinStr + ",time="+intervalStr+",lastRun="+millis()+";");
                }
                else
                {
                  UpdateSettings("@analogRead,enable=1,pin="+pinStr+",returnMaxMin=" + returnMaxMinStr + ",time="+intervalStr+",lastRun="+millis()+";");
                }
            }
            else
            {
              Out("debug RunCommands analogRead pinStr invalid : ", pinStr);
            }
          }
          if(toRun.startsWith("@digitalRead")) // toRun : @digitalRead,pin=0,time=10000,lastrun=0; 
          {
            String pinStr = GetSettingValue(toRun,"pin");
            if(pinStr.length() == 1 || pinStr.length() == 2) // this must be 1 or 2
            {
              if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))  // Get time interval to run and last run time. If time to run then run
              {
                pinMode(atoi(pinStr.c_str()), INPUT);
                String digitalValue = (String)digitalRead(atoi(pinStr.c_str()));
                Out("RunCommands",((String)"digitalRead " + (String)pinStr + " value : " + digitalValue));
                SendData("digitalRead " + pinStr,digitalValue);
                
                if(lastRunStr== "1")
                {
                  UpdateSettings("@digitalRead,enable=0,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");                  
                }
                else
                {
                  UpdateSettings("@digitalRead,enable=1,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");
                }
              }
            }
            else
            {
              Out("debug RunCommands digitalRead pinStr invalid ", pinStr);
            }
          }
          if(toRun.startsWith("@digitalWrite"))   // toRun : @digitalWrite,enable=1,pin=0,type=HIGH,time=1000,lastrun=0;
          {
            String pinStr = GetSettingValue(toRun,"pin");
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
                
                if(lastRunStr== "1")
                {
                  UpdateSettings("@digitalWrite,enable=0,pin="+pinStr+",type="+typeStr+",time="+intervalStr+",lastRun="+millis()+";");
                }
                else
                {
                  UpdateSettings("@digitalWrite,enable=1,pin="+pinStr+",type="+typeStr+",time="+intervalStr+",lastRun="+millis()+";");
                }
              }
            }
            else
            {
              Out("debug RunCommands digitalWrite pinStr invalid : ", pinStr);
            }
          } 
          if(toRun.startsWith("@sampleExecute")) // toRun @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underlow,time=10000,lastRun=0;
          {         
            String inStr = GetSettingValue(toRun,"input");
            String pinInStr = GetSettingValue(toRun,"pinIn");
            String pinOutStr = GetSettingValue(toRun,"inOut");
            String hiValStr = GetSettingValue(toRun,"highVal");
            String loValStr = GetSettingValue(toRun,"lowVal");
            String isOnStr = GetSettingValue(toRun,"isOn");
            String isOffStr = GetSettingValue(toRun,"isOff");
            String intervalStr = GetSettingValue(toRun,"time");
            String lastRunStr = GetSettingValue(toRun,"lastRun");
            if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
            {
              Out("debug RunCommands sampleExecute", "Running");
              if(inStr == "analog")
              {
                int analogValue = analogRead(atoi(pinInStr.c_str())); // 0 to 1024
                bool isAnalogBelowHigh = (analogValue < atoi(hiValStr.c_str())); 
                bool isAnalogAboveLow = (analogValue > atoi(loValStr.c_str())); 
                Out("RunCommands sampleExecute analog",((String)analogValue + " : " + (String)isAnalogBelowHigh  + " " + (String)isAnalogAboveLow));
                if(isOnStr == "overHigh" && !(isAnalogBelowHigh))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                  Out("debug sampleExecute setting pin high", pinOutStr);
                } 
                else if (isOnStr == "underLow" && !(isAnalogAboveLow))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                  Out("debug sampleExecute setting pin high", pinOutStr);
                }
                else if(isOnStr == "overUnder" && !(isAnalogAboveLow) && !(isAnalogBelowHigh)) //below low, above high
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                  Out("debug sampleExecute setting pin high", pinOutStr);
                }
                else if (isOnStr == "between" && (isAnalogBelowHigh && isAnalogAboveLow))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                  Out("debug sampleExecute setting pin high", pinOutStr);
                } 
                else if(isOffStr == "overHigh" && !(isAnalogBelowHigh)) // first check for all isOn, if isOn then don't check for isOff, else if
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                  Out("debug sampleExecute setting pin low", pinOutStr);
                }
                else if (isOffStr == "underLow" && !(isAnalogAboveLow))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                  Out("debug sampleExecute setting pin low", pinOutStr);
                }
                else if(isOffStr == "overUnder" && !(isAnalogAboveLow) && !(isAnalogBelowHigh)) //below low, above high
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                  Out("debug sampleExecute setting pin low", pinOutStr);
                }
                else if (isOffStr == "between" && (isAnalogBelowHigh && isAnalogAboveLow))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                  Out("debug sampleExecute setting pin low", pinOutStr);
                }
              }
              else if(inStr == "digital")
              {
                //digital read
                pinMode(atoi(pinInStr.c_str()), INPUT);
                bool digitalValue = digitalRead(atoi(pinInStr.c_str()));
                Out("RunCommands sampleExecute digital",((String)digitalValue));

                if(isOnStr == "high" && digitalValue == 1)
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),HIGH);
                }
                else if (isOnStr == "low" && (digitalValue == 0))
                {
                  pinMode(atoi(pinOutStr.c_str()), OUTPUT);
                  digitalWrite(atoi(pinOutStr.c_str()),LOW);
                }
              }
              UpdateSettings(espSettings[i].substring(0,espSettings[i].indexOf("lastRun="))+"lastRun="+(String)millis()+";");
            }
          }
          if(toRun.startsWith("@sendStatus"))   // @sendStatus,enable=1,net=1,sys=1,time=10000,lastRun=0;
          {
            // todo get Public IP // DNS query
            String netStr = GetSettingValue(toRun,"net");
            String sysStr = GetSettingValue(toRun,"sys"); // all system status bundled
            String updatedToRun = "@sendStatus,enable=1,"; 
            if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
            {
              if(netStr == "1")
              {
                SendData("localIP", (String)WiFi.localIP().toString());
                SendData("gatewayIP", (String)WiFi.gatewayIP().toString());;
                Out("RunCommands SendStatus net", (String)WiFi.localIP().toString());
                updatedToRun += "net=1,";
              }
              else
              {
                updatedToRun += "net=0,";
              }
              if(sysStr == "1")
              {
                SendData("uptime", String(millis() / 60000));
                SendData("status", theStatus);
                Out("RunCommands SendStatus", theStatus);
                updatedToRun += "sys=1,";
              }
              else
              {
                updatedToRun += "sys=0,";
              }
              UpdateSettings(updatedToRun+"time="+intervalStr+",lastRun="+millis()+";");
            }        
          }
          if(toRun.startsWith("@newServer")) // @newServer,host=http://server.com/esp;
          {
            String hostStr = GetSettingValue(toRun,"host");
            if(hostStr.substring(hostStr.length()-1,hostStr.length()) == "/") //last char == /
            {
              Out("debug RunCommands newServer","Last char is /");
              hostStr = hostStr.substring(0,hostStr.length()-1);
            }
            espSettings[i] = ""; //run once, a trigger setting

            hostUrl = hostStr;
          }
          if(toRun.startsWith("@system")) // @system,enable=1,newLoop=1000,accessPoint=1,httpSrv=1,update=10000,debug=1,serialRead=1,time=100000,lastRun=0;
          {
            //todo keep all settings not changed // if setting comes in as @system,enable=1,accessPoint=1; // keep previous settings and only add or update 1 in accessPoint
            String prevLoopStr = GetSettingValue(espSettings[i],"newLoop");
            String prevApStr = GetSettingValue(espSettings[i],"accessPoint");
            String prevHttpStr = GetSettingValue(espSettings[i],"httpSrv");
            String prevUpdateStr = GetSettingValue(espSettings[i],"update");
            String prevSerialStr = GetSettingValue(espSettings[i],"serialRead");
            String prevDebugStr = GetSettingValue(espSettings[i],"debug");

              String apStr = GetSettingValue(toRun,"accessPoint");
              String updateStr = GetSettingValue(toRun,"update");
              String loopStr = GetSettingValue(toRun,"newLoop");
              String httpSrvStr = GetSettingValue(toRun,"httpSrv");
              String debugStr = GetSettingValue(toRun,"debug");
              String serialStr = GetSettingValue(toRun,"serialRead");
              String systemCmdStr;
              String updatedToRun;

              if(lastRunStr == "1")
              {
                systemCmdStr = "@system,enable=0,"; 
                //Out("debug @system 0", systemCmdStr);
              }
              else
              {
                systemCmdStr = "@system,enable=1,"; 
                //Out("debug @system 1", systemCmdStr);
              }

              if(atoi(updateStr.c_str()) > 1)
              {
                UpdateSettings("@update,enable=1,time="+updateStr+",lastRun="+millis());
              }
              
              if(httpSrvStr == "1")
              {
                updatedToRun += "httpSrv=1,";
                behttpSrv = true;
                setuphttpSrv();
              }
              else if (httpSrvStr == "0")
              {
                updatedToRun += "httpSrv=0,";
                behttpSrv = false;
              }
              else if (prevHttpStr != "")
              {
                updatedToRun += "httpSrv=" + prevHttpStr + ","; //no incoming, set to last?
              }

              if(apStr == "1")
              {
                updatedToRun += "accessPoint=1,";
                SetupAP(true);
              }
              else if (apStr == "0")
              {
                updatedToRun += "accessPoint=0,";
                SetupAP(false);         
              }
              else if (prevApStr != "")
              {
                updatedToRun += "accessPoint=" + prevApStr + ","; //no incoming, set to last
              }

              Out("debug RunCommands debugStr", debugStr);
              if(debugStr == "1")
              {
                updatedToRun += "debug=1,";
                isDebugOut = true;
              }
              else if(debugStr == "0")
              {
                updatedToRun += "debug=0,";
                isDebugOut = false;
              }
              else if (prevApStr != "")
              {
                updatedToRun += "accessPoint=" + prevApStr + ","; //no incoming, set to last
              }

              Out("debug RunCommands serialStr", serialStr);
              if(serialStr == "1")
              {
                //updatedToRun += "serialRead=1,";
                UpdateSettings("@serialRead,enable=1,lastRun=1;");
              }
              else if(serialStr == "0")
              {
                //updatedToRun += "serialRead=0,";
                UpdateSettings("@serialRead,enable=0,lastRun=1;");
              }
              else if(prevSerialStr != "")
              {
                //updatedToRun += "serialRead=" + prevSerialStr + ",";
                //UpdateSettings("@serialRead,enable="+prevSerialStr+";");
              }

              long newLoopInterval = atoi(loopStr.c_str());
              if(loopInterval != newLoopInterval && newLoopInterval >= 1)
              {
                loopInterval = newLoopInterval;
                updatedToRun += "newLoop="+loopStr+","; 
                Out("debug RunCommands loop set ", (String)loopStr);
              }
              else
              {
                updatedToRun += "newLoop="+(String)loopInterval+",";
              }

              UpdateSettings(systemCmdStr + updatedToRun+"time="+intervalStr+",lastRun="+millis()+";");            
          }
          if(toRun.startsWith("@network")) //@network,enable=1,ssdp=1,mdns=1,llmnr=1,lastRun=0; accessPoint,httpSrv?
          {
            String ssdpStr = GetSettingValue(toRun,"ssdp");
            String mdnsStr = GetSettingValue(toRun,"mdns");
            String llmnrStr = GetSettingValue(toRun,"llmnr");
            String dnsHostStr = GetSettingValue(toRun,"dnsHost");

            if(ssdpStr == "1" || mdnsStr == "1" || llmnrStr == "1")
            {
              beNetSrv = true;
              setupNet();
            }
            if(dnsHostStr == "1")
            {
              //todo host controlUrl
            }
            /*if(scanStr == "1")
            {
              //scan for like kind
            }*/
            
            espSettings[i] = ""; //trigger setting
          }
          if(toRun.startsWith("@update")) //@update,enable=1,time=10000,lastRun=0;
          {            
              Out("debug RunCommandsupdate ", "Running GetUpdateFromHost()");
              if(GetUpdateFromHost())
              {
                Out("RunCommands GetUpdateFromHost returned", "true"); //GetUpdateFromHost returns true if get @update
              }
              else
              {
                if(lastRunStr == "1")
                {
                  UpdateSettings("@update,enable=0,time="+intervalStr+",lastRun="+millis()+";"); 
                }
                else
                {
                  UpdateSettings("@update,enable=1,time="+intervalStr+",lastRun="+millis()+";"); 
                }             
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
                  theStatus = "Serial config.";
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
}

void SetupAP(bool beAP) {
  if(!apInitialized && beAP)
  { 
    apInitialized = true;
    WiFi.mode(WIFI_AP_STA);
    delay(50);
    WiFi.softAP(("ESP_" + (String)(ESP.getChipId())).c_str(), "");
    Out("debug SetupAP","AP Station mode");
  }
  else if(apInitialized && !beAP)
  {
    apInitialized = false;
    WiFi.mode(WIFI_STA);
    delay(50);
    Out("debug SetupAP","station mode");
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
