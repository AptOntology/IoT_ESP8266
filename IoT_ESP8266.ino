/**
ESP8266 Client

1. Connects to wifi
2. Gets settings from either initial hardcoded, serial input, internal web site input, or internet HTTP server
3. Run the settings on the given inverval
4. If no wifi then become access point for local control

Initial Settings:
@system,enable=1,newLoop=1000,accessPoint=0,httpSrv=1,update=200000,debug=1,serialRead=1,time=10000,lastRun=0;

Settings
analog input to digital output
supports isOn/isOff in states overHigh, underLow, overUnder, and between the highVal and lowVal, if isOn(on) elseif isOff(off)
Example (On over highVal, off under lowVal) : @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=1000,lowVal=500,isOn=overHigh,isOff=underLow,time=10000,lastRun=0;
Example (On between, off when under minimum) : @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=1024,lowVal=1000,isOn=between,isOff=underLow,time=10000,lastRun=0;
Example (On between, off over high and under low) : @sampleExecute,input=analog,pinIn=A0,pinOut=12,highVal=800,lowVal=400,isOn=between,isOff=overUnder,time=1000,lastRun=0;

digital input to digital output:
Example (trigger): @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=high,isOff=no,time=10000,lastRun=0;

Example (flopflip): @sampleExecute,input=digital,pinIn=10,pinOut=12,isOn=low,isOff=high,time=1000,lastRun=0;

@analogRead,enable=1,pin=A0,returnMaxMin=1,time=100000,lastRun=0;
@digitalRead,enable=1,pin=12,time=30000,lastRun=0;
@digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;
@SendStatus,enable=1,net=1,sys=1,time=10000,lastRun=0;
@system,enable=1,newLoop=500,accessPoint=0,httpSrv=0,update=200000,debug=0,serialRead=0,time=100000,lastRun=0; // production mode?
@network,mdns=1,ssdp=1,llmnr=1,dnsHost=1; // todo, dnsHost cmd to dish out the new hostURL when offline?
@update,enable=1,time=300000,lastRun=0; //get update from http server
@serialRead,enable=1; 

Todo 
mesh/standalone mode
If no initial wifi, scan for open ESP_ID### AP, connect, get settings, and run commands function as usual. 
If no ESP_ID, be ap, dns(hostURL), and http server and dish out settings through hostURL/control.php. Receive by data.php to array, Store&forward
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
bool GetUpdateFromHost(); // Called from @update command in RunSettings
void UpdateSettings(String line); // Updates espSettings array when settings run or get setup
void SendData(String command, String data); // Sends chipId, command, data to http get and adds to lastFiveSent[][] array
void RunCommands(); // Main 
void SetupAP(bool beAP); // Setup / Tear down an Access Point 
void Out(String function, String message); // Write serial output

// Function declarations external files
void LoophttpSrv(); // httpSrv.ino 
void setuphttpSrv();
void setupNet();
void loopNet();

//Global variables
String hostUrl = "https://www.YourServerUrl.net/esp"; // 

int programVersion = 1;
String lastFiveSent[2][5]; // [1][x]=command,[0][x]=result, [2][x]=millis()?
int loopInterval = 1000; // delay between action
bool isDebugOut = true; //Enable verbose debug logging
bool autonomous; //experimental
bool apInitialized = false;
bool isSettingsInit = false;; //initSettings()
bool behttpSrv = false; //enabled through @httpSrv setting
bool beNetSrv = false; //enabled through @network setting
int sendDataCount=0; //SendData()
String theStatus; // system status for internal web server
String espSettings[20];

//Functions
void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("On!");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(STASSID, STAPSK);
  Out("setup","Add ssid...'" STASSID "'");
}

void loop() {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    RunCommands();
  }
  else
  {
    // No WIfi? be autonomous AP for a while 
    isDebugOut = autonomous = true; 
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

void initSettings()
{
  espSettings[0] = "@system,enable=1,newLoop=1000,accessPoint=0,httpSrv=1,update=200000,debug=1,serialRead=1,time=10000,lastRun=0;";
  theStatus = "Initialized.";
  Out("debug initSettings IP", (String)WiFi.localIP().toString());
  isSettingsInit = true;
}

String GetSettingValue(String toRun, String name) 
{ 
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

    if(theCmd.indexOf("@update") != -1)  // if the command is update then the server is updating @update. Used in RunSettings @update to check if @update needs UpdateSettings for LastRun
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

void UpdateSettings(String line)
{
  if(line != "")
  {
    for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
    {
      String settingName = line.substring(line.indexOf("@")+1, line.indexOf(","));
      String pinStr = GetSettingValue(line,"pin");
      if (espSettings[i].indexOf(settingName) != -1 && (pinStr == "" || espSettings[i].indexOf("pin="+pinStr) != -1)) //if incoming pin is empty or pin matches setting then update
      {
        // Three states:0,1,2 : update to incoming lastrun = 0, update setting and keep previous last run, update setting and set lastrun to millis() 
        // if incoming lastRun = 0 then set to 0, update lastRun to now millis() if greater than 10, and set to previous lastRun if between 1-10 (allowing to change settings and keep next run time)
        // The server sends @update command with lastRUn value greater than 10 and then UpdateSettings sets the last run to the current run time
        if(line.indexOf("lastRun") > 0)
        {
          Out("debug UpdateSettings update ", line);
          String incomingLastRun = GetSettingValue(line,"lastRun");
          if(incomingLastRun == "0") // update as-is, runs on next interval
          {
            espSettings[i] = line; //update existing setting
          }
          else if(atoi(incomingLastRun.c_str()) < 10) // if less than 10 then set to the previous run as though the command didnt run, this is useful to update the command setting without changing the actual run interval
          {
            String previousRun = GetSettingValue(espSettings[i],"lastRun");
            //todo check for previousRun to be?
            espSettings[i] = line.substring(0,line.indexOf("lastRun=")) + "lastRun="+previousRun+";";
          }
          else //else update LastRun with the current millis // incoming last run greater than 10 indicates we want to change it to now as it just ran or otherwise
          {
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
    Out("debug UpdateSettings Line empty ", line);
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
  if(!(isSettingsInit)) { initSettings(); }  // if settings empty then initialize, could this be in setup()?
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
              Out("RunCommands",((String)"analogRead " + (String)pinStr + " maxMin : " + returnMaxMinStr + " value : " + analogValue));
              if(analogValue != "1024" && analogValue != "0")
              {
                SendData("analogRead",analogValue);
              } 
              else if(returnMaxMinStr == "1")
              {
                SendData("analogRead",analogValue);
              }
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
          String intervalStr = GetSettingValue(toRun,"time"); 
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          if(pinStr.length() == 1 || pinStr.length() == 2) // this must be 1 or 2
          {
            if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))  // Get time interval to run and last run time. If time to run then run
            {
              pinMode(atoi(pinStr.c_str()), INPUT);
              String digitalValue = (String)digitalRead(atoi(pinStr.c_str()));
              Out("RunCommands",((String)"digitalRead " + (String)pinStr + " value : " + digitalValue));
              SendData("digitalRead",digitalValue);
              UpdateSettings("@digitalRead,enable=1,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");
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
          // Internal and Public IP ? 
          // DNS query
          String netStr = GetSettingValue(toRun,"net");
          String sysStr = GetSettingValue(toRun,"sys"); // all system status bundled
          String intervalStr = GetSettingValue(toRun,"time");
          String lastRunStr = GetSettingValue(toRun,"lastRun");
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
              Out("RunCommands SendStatus", "sys");
              updatedToRun += "sys=1,";
            }
            else
            {
              updatedToRun += "sys=0,";
            }
            if(intervalStr == "") { intervalStr = "450000";} // if no interval then set to 45 seconds
            UpdateSettings(updatedToRun+"time="+intervalStr+",lastRun="+millis()+";");
          }        
        }
        if(toRun.startsWith("@newServer")) // @newServer,host=http://server.com/esp;
        {
          String hostStr = GetSettingValue(toRun,"host");
          if(hostStr.substring(hostStr.length()-1,hostStr.length()) == "/") //last char == / ?
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
          String intervalStr = GetSettingValue(toRun,"time");
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
          {
            String apStr = GetSettingValue(toRun,"accessPoint");
            String updateStr = GetSettingValue(toRun,"update");
            String loopStr = GetSettingValue(toRun,"newLoop");
            String httpSrvStr = GetSettingValue(toRun,"httpSrv");
            String debugStr = GetSettingValue(toRun,"debug");
            String serialStr = GetSettingValue(toRun,"serialRead");

            String updatedToRun = "@system,enable=1,"; 
            
            if(atoi(updateStr.c_str()) > 1)
            {
              UpdateSettings("@update,enable=1,time="+updateStr+",lastRun="+millis());
              //updatedToRun = "update=0," // run once - trigger
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
            
            if(serialStr == "1")
            {
              UpdateSettings("@serialRead,enable=1;");
            }

            long newLoopInterval = atoi(loopStr.c_str());
            if(loopInterval != newLoopInterval && newLoopInterval >= 1)
            {
              loopInterval = newLoopInterval;
              updatedToRun += "loop="+loopStr+","; //loop=1000,
              Out("debug RunSettings loop set ", (String)loopStr);
            }
            else
            {
              updatedToRun += "loop="+(String)loopInterval+","; //loop=1000,
              //Out("debug RunSettings loop not set ", (String)loopStr);
            }

            if(intervalStr == "") { intervalStr = "450000";} // if no interval then set to 45 seconds
            UpdateSettings(updatedToRun+"time="+intervalStr+",lastRun="+millis()+";");
          }
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
          String intervalStr = GetSettingValue(toRun,"time");
          String lastRunStr = GetSettingValue(toRun,"lastRun");
          if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
          {
            Out("debug RunCommandsupdate ", "Running GetUpdateFromHost()");
            // Perhaps change the name to GetUpdate
            if(GetUpdateFromHost())
            {
              Out("RunCommands GetUpdateFromHost returned", "true");
              // The server sends @update command with lastRUn value greater than 10 and then UpdateSettings sets the last run to the current run time, thus no UpdateSettings here
            }
            else
            {
              // no @update from server, update the LastRun to now and keep the current intervalStr
              if(intervalStr == "") { intervalStr = "450000";} // if no interval then set to 45 seconds
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

void beAutonomous()
{
  // check for device, identify function, update settings, run commands
  //Read analog pin A0
  //if analog read is not equal default, for a given time or
  //if a0 is > or < a given threshold or
  //if a0 is bouncing over a period of time
  //Read digital pins
  //or just use random number switchCase to enable random settings and see what happens
  switch((int)ESP.random(0,10))
  {
    case 0:
    {
      UpdateSettings("@system,enable=1,accessPoint=1;");
    }
    case 1:
    {
      UpdateSettings("@system,enable=1,debug=1;");  
    }
    case 2:
    {
      UpdateSettings("@system,enable=1,httpSrv=1;"); 
    }
    case 3:
    {
      UpdateSettings("@system,enable=1,httpSrv=1;"); 
    }
    case 4:
    {
      UpdateSettings("@sendStatus,enable=1,net=1,sys=1;"); 
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
