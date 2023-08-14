/**
ESP8266 Client

connects to wifi, get unique settings from either initial, serial input, internal web server input, or internet HTTP /control.php?chipid=ESPID, run the settings on the given inverval

Initial Settings:
@update,enable=1,time=300000,lastRun=0;
@serialRead,enable=1;
@debug,enable=1,true;

Supported Settings
@analogRead,enable=1,pin=A0,time=30000,lastRun=0;
@digitalRead

Note: setting order shouldn't matter, code logic finds the name

Native libraries 
V 0.2

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
// 60000 millis = 1 min

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
  }
  delay(loopInterval);

  LoopAP(); // Be an AP if enableAP else handle web server?
}

void diag() {
 String gatewayIp = WiFi.gatewayIP().toString();
 String localIp = WiFi.localIP().toString();
 Out("diag gateway ip", gatewayIp);
 Out("diag local ip", localIp);
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
          String incomingLastRun1 = line.substring(line.indexOf("lastRun="),line.indexOf(";")); // @cmd,pin=1,time=1,lastRun=123;
          String incomingLastRun = incomingLastRun1.substring(incomingLastRun1.indexOf("=")+1,incomingLastRun1.indexOf(";")); //123
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

void RunCommands() {
    // @update,enable=1,true;
    // @anlogRead,pin=1,enable=0,time=100,lastRun=9;
    // @debug,enable=0;
    if(espSettings[0].indexOf(",") < 1) { 
      espSettings[0] = "@update,enable=1,time=300000,lastRun=0;"; 
      espSettings[1] = "@serialRead,enable=1;"; 
      espSettings[2] = "@debug,enable=1,false;";
      } // if empty add update
    for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
    {
        if (espSettings[i] != "")
        {
          String toRun = espSettings[i];
          String enableStr1 = (toRun.substring(toRun.indexOf("enable"), toRun.indexOf(";"))); //enable=1,time=30000,lastRun=0;
          String enableStr = (enableStr1.substring(enableStr1.indexOf("=")+1,enableStr1.indexOf("=")+2)); // 1
          if(enableStr == "1" || enableStr != "0")
          {
            Out("debug RunCommands enabled ",toRun);
            if(toRun.startsWith("@analogRead")) 
            {
              // toRun :  @analogRead,enable=1,pin=A0,time=10,lastRun=0;

              // todo
              //String pinStr1 = toRun.substring(toRun.indexOf("pin"), toRun.indexOf(",", toRun.indexOf("pin")));
              //String intervalStr1 = toRun.substring(toRun.indexOf("time"), toRun.indexOf(",", toRun.indexOf("time")));
              //String lastRunStr1 = toRun.substring(toRun.indexOf("lastRun"), toRun.indexOf(";", toRun.indexOf("lastRun")));
              //String pinStr1 = toRun.substring(toRun.indexOf("pin"),toRun.indexOf(";")); // pin=0,time=10,lastRun=0;
              //String pinStr = pinStr1.substring(pinStr1.indexOf("=")+1,pinStr1.indexOf(",")); // 0
              String pinStr2 = toRun.substring(toRun.indexOf("pin"), toRun.indexOf(",", toRun.indexOf("pin"))); //pin=A0
              String pinStr = pinStr2.substring(pinStr2.indexOf("=")+1,pinStr2.length()); // A0

              if(pinStr.indexOf("A") != -1) // this must start wtih A
              {
                //interval time
                String intervalStr1 = toRun.substring(toRun.indexOf("time"),toRun.indexOf(";")); // time=10,lastrun=0;
                String intervalStr = intervalStr1.substring(intervalStr1.indexOf("=")+1,intervalStr1.indexOf(",")); // 10

                //last run time
                String lastRunStr1 = toRun.substring(toRun.indexOf("lastRun"),toRun.indexOf(";")); // lastRun=0;
                String lastRunStr = lastRunStr1.substring(lastRunStr1.indexOf("=")+1,lastRunStr1.indexOf(";")); // 0

                // Get time interval to run and last run time. If time to run then run
                if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
                {
                  String analogValue = (String)analogRead(atoi(pinStr.c_str()));
                  Out("RunCommands",((String)"analogRead " + (String)pinStr + " value : " + analogValue));
                  if(analogValue != "1024" && analogValue != "0")
                  {
                    SendData("analogRead",analogValue);
                  }
                  UpdateSettings("@analogRead,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");
                }
              }
              else
              {
               Out("debug RunCommands analogRead pinStr no A : ", pinStr);
              }
            }
            if(toRun.startsWith("@digitalRead"))
            {
              // toRun : @digitalRead,pin=0,time=1000,lastrun=12345;
              String pinStr1 = toRun.substring(toRun.indexOf("pin"),toRun.lastIndexOf(";")); // pin=0,time=1000/t,lastrun=12345;
              String pinStr = pinStr1.substring(pinStr1.indexOf("=")+1,pinStr1.indexOf(",")); // 0
              pinMode(atoi(pinStr.c_str()), INPUT);
              String analogValue = (String)digitalRead(atoi(pinStr.c_str()));
              Out("RunCommands",((String)"digitalRead " + (String)pinStr + " value : " + analogValue));
              SendData("digitalRead",analogValue);
            }
            if(toRun.startsWith("@digitalWrite"))               // toRun : @digitalWrite,pin=0,type=HIGH,time=1000,lastrun=12345;
            {
              String pinStr1 = toRun.substring(toRun.indexOf("pin"),toRun.lastIndexOf(";")); // pin=0,time=1000/t,lastrun=12345;
              String pinStr = pinStr1.substring(pinStr1.indexOf("=")+1,pinStr1.indexOf(",")); // 0//
              String typeStr1 = toRun.substring(toRun.indexOf("type"),toRun.lastIndexOf(";")); // type=HIGH,time=1000,lastrun=12345;
              String typeStr = typeStr1.substring(typeStr1.indexOf("=")+1,typeStr1.indexOf(",")); // HIGH
            
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
            if(toRun.startsWith("@loop"))
            {
              // toRun : @loop,time=10000,enable=1;
              // todo: add enable, refactor to include time string rather than = in the next line
              int newLoopInterval = atoi((toRun.substring(toRun.lastIndexOf("=")+1,toRun.length())).c_str());
              //Out("debug RunCommand Loop interval", ("Loop interval set to "+ (String)newLoopInterval));
              if(loopInterval != newLoopInterval)
              {
                loopInterval = newLoopInterval;
              }
            }
            if(toRun.startsWith("@update")) //@update,enable=1,time=10000,lastRun=0;
            {            
              //interval time
              String intervalStr1 = toRun.substring(toRun.indexOf("time"),toRun.indexOf(";")); // time=10,lastrun=0;
              String intervalStr = intervalStr1.substring(intervalStr1.indexOf("=")+1,intervalStr1.indexOf(",")); // 10

              //Out("debug RunCommandsupdate intervalStr1", intervalStr1);
              //Out("debug RunCommandsupdate intervalStr", intervalStr);

              //last run time
              String lastRunStr1 = toRun.substring(toRun.indexOf("lastRun"),toRun.indexOf(";")); // lastRun=0;
              String lastRunStr = lastRunStr1.substring(lastRunStr1.indexOf("=")+1,lastRunStr1.indexOf(";")); // 0

              // Get time interval to run and last run time. If time to run then run
              if(millis() > atoi(lastRunStr.c_str()) + atoi(intervalStr.c_str()))
              {
                Out("debug RunCommandsupdate ", "Running GetSetupConfig()");
                if(GetSetupConfig())
                {
                  // received update
                  //Out("debug RunCommands GetSetupConfig returned true", " ");
                }
                else
                {
                  // no @update from server, update the LastRun to now and keep the current intervalStr
                  //Out("debug RunCommands GetSetupConfig returned false", " ");
                  if(intervalStr == "") { intervalStr = "45000";} // if no interval then set to 45 seconds
                  UpdateSettings("@update,enable=1,time="+intervalStr+",lastRun="+millis()+";");
                  //UpdateSettings("@analogRead,pin="+pinStr+",time="+intervalStr+",lastRun="+millis()+";");
              
                }
              }
            }
            if(toRun.startsWith("@debug"))               //@debug,enable=1,mode=false;
            {
              if(toRun.indexOf("false") > 0)
              {
                isDebugOut = false;
              }
              else 
              {
                isDebugOut = true;
              }
            }
            if(toRun.startsWith("@serialRead"))
            {
              if(Serial.available())
              {
                Out("RunCommands","Reading serial...");
                int x;
                String readStr = "";

                  readStr = Serial.readString();
                  delay(2);
                  x=x++;
                  if(readStr != "")
                  {
                    readStr = readStr.substring(0,readStr.length()-1);
                    readStr.trim();
                    Out("RunCommands serialRead",readStr);
                    if(readStr.startsWith("@"))
                    {
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

    if(theCmd.indexOf("@update") != -1) 
    {
      toReturn = true;
    }
    if(theCmd.indexOf("0") != 0) // filter this 
    {
      UpdateSettings(theCmd);
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
