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
@analogRead,enable=1,pin=A0,time=30000,lastRun=0;
@digitalRead,enable=1,pin=12,time=30000,lastRun=0;
@digitalWrite,enable=1,pin=12,type=HIGH,time=1000,lastrun=0;
@AccessPoint,enable=1,be=true; 
@update,enable=1,time=300000,lastRun=0; //get update from http server
@loop,enable=1,time=1000; // this controls interval between RunSettings in loop()
@serialRead,enable=1; 
@debug,enable=1,be=true;

Todo 
support for multiple @analogRead, @digitalRead per pin, currently allows 1
settings to control internal web services separate from access point
output setting results to internal html page for realtime display 

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

// Functions
void Out(String function, String message);
bool GetSetupConfig();

// Global vars
const String & hostUrl = "https://www.YourServerUrl.net/esp"; // 

int loopInterval = 2000; // delay between action 
bool isDebugOut = true; //Enable verbose debug logging
bool runOnce = false;
bool enableAP = false; 
bool autonomous; 
String theStatus; // short status update for instant dashboard reporting? appends status?

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("On!");
  Out("setup","Connecting to ssid...'" STASSID "'");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(STASSID, STAPSK);
  SetupAP();
}

void loop() {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    if(runOnce != true) { diag(); runOnce = true; }
    RunCommands();
  }
  else
  {
    // No WIfi? be autonomous AP for a while 
    isDebugOut = autonomous = enableAP = true; 
    theStatus = "No Wifi!";
  }
  delay(loopInterval);
  LoopAP(); // Be an AP if enableAP else handle web server?
}

void diag() {
  // multi-diag for setup() through RunCommands? with output to serial and wifi SendOutput()?
}

String espSettings[20];
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
        // if incoming lastRun = 0 then set to 0, update lastRun cycle with millis if between 1-10, and set explicit if greater than 10
        if(line.indexOf("lastRun") > 0)
        {
          //String incomingLastRun1 = line.substring(line.indexOf("lastRun="),line.indexOf(";")); // @cmd,pin=1,time=1,lastRun=123;
          //String incomingLastRun = incomingLastRun1.substring(incomingLastRun1.indexOf("=")+1,incomingLastRun1.indexOf(";")); //123
          String incomingLastRun = GetSettingValue(line,"lastRun");
          if(incomingLastRun == "0") // update as-is, always runs on next interval
          {
            Out("debug UpdateSettings incomingLastRun ", (String)incomingLastRun);
            espSettings[i] = line; //update existing setting
          }
          else if(atoi(incomingLastRun.c_str()) < 10) // if less than 10 then continue with the current lastRun cycle, useful for update command so it's not continually cycling
          {
           // long theTime = millis();
           // espSettings[i] = line.substring(0,line.indexOf("lastRun="))+"lastRun="+theTime+";";
            String previousRun1 = espSettings[i].substring(espSettings[i].indexOf("lastRun="),espSettings[i].indexOf(";")); // @cmd,pin=1,time=1,lastRun=123;
            String previousRun = previousRun1.substring(previousRun1.indexOf("=")+1,previousRun1.indexOf(";")); //123
           espSettings[i] = line.substring(0,line.indexOf("lastRun=")) + "lastRun="+previousRun+";";
           
          }
          else //else update LastRun with incoming // incoming last run greater than 10 indicates we want to change it now
          {
            Out("debug UpdateSettings incomingLastRun > 10 ", (String)incomingLastRun);
            long theTime = millis();
            //espSettings[i] = line;
            espSettings[i] = line.substring(0,line.indexOf("lastRun="))+"lastRun="+theTime+";";  
          }
        }
        else
        {
          Out("debug UpdateSettings update existing, no LastRun : ", ((String)line + " : " + (String)i));
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
}

// can this logic be abstracted into a function ?
String GetSettingValue(String toRun, String name) //incoming toRun and name of command (pin,time,lastRun,etc)
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
bool isSettingsInit; 
void initSettings()
{
  espSettings[0] = "@update,enable=1,time=300000,lastRun=0;"; 
  espSettings[1] = "@serialRead,enable=1;"; 
  espSettings[2] = "@debug,enable=1,be=false;";
  theStatus = "Initialized!";
  isSettingsInit = true;
}

void RunCommands() {
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
            if(toRun.startsWith("@digitalRead")) // toRun : @digitalRead,pin=0,time=1000,lastrun=12345;
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
            if(toRun.startsWith("@AccessPoint")) //@AccessPoint,enable=1,be=true; // does this need an interval or can we flipflop the boolean?
            {
              String beStr = GetSettingValue(toRun,"be");
              if(beStr == "true")
              {
                if(!enableAP)
                {
                  enableAP = true;
                  SetupAP();
                  Out("debug RunSettings AccessPoint", "true");
                }
              }
              else
              {
                enableAP = false;
                WiFi.mode(WIFI_STA);
                Out("debug RunSettings AccessPoint", "false");
              }
            }
            if(toRun.startsWith("@loop")) //@loop,enable=1,time=1000;
            {
              String intervalStr = GetSettingValue(toRun,"time");
              long newLoopInterval = atoi(intervalStr.c_str());
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
                if(GetSetupConfig())
                {
                  Out("debug RunCommands GetSetupConfig returned", "true");
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
                    UpdateSettings(readStr);
                    theStatus = "Configured by serial!"; // ??TODO??
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
  bool toReturn = false;
  String chipID = (String)ESP.getChipId();
  String controlUrl = hostUrl + "/control.php/?chipid="+chipID;
  String payload = GetHttpResponse(controlUrl);
  Out("debug GetSetupConfig url", controlUrl);
  Out("debug GetSetupConfig payload", payload);

  /* 
    payload is a String
    @aR,pin0,time3;
    @dR,pin12,time3;
    @update;
    @debug;
  */
  int loopLength = 0;
  int atIndex = 0;
  int semiIndex = 0;
  String subPayload = payload;
  //calculate amount of lines in payload, add up the semicolon
  String payloadTemp = payload; 
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
    //Out("deubg GetSetupConfig atIndex",(String)atIndex);
    //Out("deubg GetSetupConfig semiIndex",(String)semiIndex);
    //Out("deubg GetSetupConfig subPayload",(String)subPayload);

    // at index should always be 1, semiIndex should always be the length of the string, the last entry may or may not have a ; (lol)
    String theCmd = subPayload.substring(atIndex,semiIndex+1);

    if(theCmd.indexOf("@update") != -1)  // if the command is not update then toReturn is true? I know I put this here for some reason though... todo
    {
      toReturn = true; 
    }
    if(theCmd.indexOf("@") == 0) // every setting must start with @, if >= or otherwise then its broken setting, could also check for trailing ; while we're here
    {
      UpdateSettings(theCmd);
      theStatus = "Configured by server!";
      toReturn = true;
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
