/*
ESP Web Services

*/
#include <ESP8266WebServer.h>
// Objects
ESP8266WebServer server(80);
// Function declarations
void setuphttpSrv();
void LoophttpSrv();
String macToString(const unsigned char* mac);
String getHead();
String getFoot();
void handleRoot();
void handleControl();
void handleOutput();
void handleInfo();
void handleSettings();
void handleScanNetworks();
void handleConnectWifi();
//Global variables
bool httpSrvInit = false; 
// Functions
void setuphttpSrv()
{
  if(!httpSrvInit)
  {
    server.on("/", handleRoot);
    server.on("/scan.html", handleScanNetworks);
    server.on("/connect.html", handleConnectWifi); 
    server.on("/control.html", handleControl); 
    server.on("/settings.html", handleSettings);
    server.on("/info.html", handleInfo);
    server.on("/output.html", handleOutput);
    server.begin();
    httpSrvInit = true;
  }
}

void LoophttpSrv()
{
//  setuphttpSrv(); // perf gain by not ensuring setup each request?
  server.handleClient();
}

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String getHead()
{
  //<head> <meta http-equiv='refresh' content='15'>
  String htmlHead2;
  String htmlHead1 = "<html> <head> <style> .flex-container { display: -webkit-flex; display: flex; -webkit-flex-flow: row wrap; flex-flow: row wrap; text-align: center; } .flex-container > * { padding: 15px; -webkit-flex: 1 100%; flex: 1 100%; } .article { text-align: left; } header {background: black;color:white;} footer {background: #aaa;color:white;} .nav {background:#eee;} .nav ul { list-style-type: none; padding: 0; } .nav ul a { text-decoration: none; } @media all and (min-width: 768px) { .nav {text-align:left;-webkit-flex: 1 auto;flex:1 auto;-webkit-order:1;order:1;} .article {-webkit-flex:5 0px;flex:5 0px;-webkit-order:2;order:2;} footer {-webkit-order:3;order:3;} } </style> </head> <body> <div class='flex-container'> <header> <h1>ESP_";
  htmlHead1 += (String)(ESP.getChipId());
  htmlHead1 += "</h1></header> <nav class='nav'> <ul> ";
  htmlHead2 += "<li><h2><a href='/'>Root /</a></h2></li> "; 
  htmlHead2 += "<li><h2><a href='/settings.html'>Control</a></h2></li> "; 
  htmlHead2 += "<li><h2><a href='/info.html'>Status</a></h2></li> "; 
  htmlHead2 += "<li><h2><a href='/output.html'>Output</a></h2></li> ";
  String htmlHead3 = "</ul> </nav> <article class='article'>";
  String htmlHead = htmlHead1 + htmlHead2 + htmlHead3;
  return htmlHead;
}

String getFoot()
{
  String htmlFoot = "</article> <footer> Have a nice day </footer></div></body></html>";
  return htmlFoot;
}

void handleRoot() { 
 String htmlOut = getHead();
 htmlOut += "<h2>Hello!</h2><b> I am " + (String)WiFi.hostname();
 htmlOut += ".</b>";
  if (WiFi.status() == WL_CONNECTED)
  {
    htmlOut += "<p>Connected to ";
    htmlOut += WiFi.SSID();
    htmlOut += "</p>";
  }

  // Server hostUrl status?
  // Last status or any error on SendData ?
  htmlOut += "<p>On for ";
  htmlOut += String(millis() / 60000);
  htmlOut += " minutes.</p>";

  htmlOut += "<p>";
  htmlOut += theStatus;
  htmlOut += "</p>";

  htmlOut += getFoot();
  server.send(200, "text/html", htmlOut);
  delay(30); //rest easy
}

void handleControl()
{
  Out("debug handleControl serverarg0", (String)server.arg(0));
  if(server.arg(0).indexOf("autoMode") >= 0)
  {
    if(autonomous)
    {
      autonomous = false; 
    }
    else
    {
      autonomous = true; 
    }
  }
  if(server.arg(0).indexOf("argsAnalogRead") >= 0)
  {
    Out("debug handleControl argsAnalogRead arg 0", (String)server.arg(0));
    Out("debug handleControl argsAnalogRead arg 1", (String)server.arg(1));
    Out("debug handleControl argsAnalogRead arg 2", (String)server.arg(2));
    Out("debug handleControl argsAnalogRead arg 3", (String)server.arg(3)); //time
    UpdateSettings("@analogRead,enable="+(String)server.arg(2)+",pin="+(String)server.arg(1)+",time=" + (String)server.arg(3) + ",lastRun=0;");
    //Out("debug handleControl argsAnalogRead size", (String)(sizeof(server.arg) / sizeof(server.arg[0])));
  }
    if(server.arg(0).indexOf("argsDigitalRead") >= 0)
  {
    Out("debug handleControl argsDigitalRead arg 0", (String)server.arg(0));
    Out("debug handleControl argsDigitalRead arg 1", (String)server.arg(1)); //pin
    Out("debug handleControl argsDigitalRead arg 2", (String)server.arg(2)); //enable
    Out("debug handleControl argsDigitalRead arg 2", (String)server.arg(3)); //enable
    UpdateSettings("@digitalRead,enable="+(String)server.arg(2)+",pin="+(String)server.arg(1)+",time=" + (String)server.arg(3) + ",lastRun=0;");
    //Out("debug handleControl argsAnalogRead size", (String)(sizeof(server.arg) / sizeof(server.arg[0])));
  }
  if(server.arg(0).indexOf("wifidisconnect") >= 0)
  {
    WiFi.disconnect();
  }
  if(server.arg(0).indexOf("resetHeap") >= 0)
  {
    ESP.resetHeap();
  }
  if(server.arg(0).indexOf("fullreset") >= 0)
  {
    ESP.reset();
  }
  if(server.arg(0).indexOf("enableAP") >= 0)
  {
    if(!apInitialized) // if ap is not init then enable it 
    {
      UpdateSettings("@system,enable=1,accessPoint=1,time=10000,lastRun=0;"); 
    }
    else
    {
      //apInitialized = false;
      UpdateSettings("@system,enable=1,accessPoint=0,time=10000,lastRun=0;"); 
    }
  }

  if(server.arg(0).indexOf("@") == 0) //
  {
    Out("handleControl",server.arg(0));
    UpdateSettings(server.arg(0));
  }

  if(server.arg(0).indexOf("debug") >= 0) //
  {
    if(isDebugOut)
    {
      UpdateSettings("@system,enable=1,debug=0,time=10000,lastRun=0;");
    }
    else
    {
      UpdateSettings("@system,enable=1,debug=1,time=10000,lastRun=0;");
    }
  }
  handleSettings();
}

void handleOutput()
{
  String htmlOut = getHead();
  htmlOut += "<table style='width:100%'><tbody>";
  htmlOut += "<tr><td><b>Command</b></td><td><b>Result</b></td><td></td></tr>";
 
  for(int x=0; x<=4; x++) 
  {
    htmlOut += "<tr>";
    htmlOut += "<td>";
    htmlOut += lastFiveSent[0][x];
    htmlOut += "</td><td>";
    htmlOut += lastFiveSent[1][x];
    htmlOut += "</td>";
    //htmlOut += "</td><td>";
    //htmlOut += lastFiveSent[2][x]; // todo the time 
    htmlOut += "</td>";
    htmlOut += "</tr>";
  }

  htmlOut += "</tbody></table>";
  htmlOut += getFoot();
  server.send(200, "text/html", htmlOut);
  delay(30); //rest easy
}

void handleInfo()
{
    //abstract htmlOut to a funtion?
    String htmlOut = getHead();
    htmlOut += "<table style='width:100%'><tbody>";
    htmlOut += "<tr><td><b>Description</b></td><td><b>Details</b></td></tr>";
    htmlOut += "<tr><th>Status</th><th> </th>";
    htmlOut += "<tr><td>Ready</td><td>";
    if((millis() / 60000) > 1)
    {
      if(theStatus != "Initialized.")
      {
        htmlOut += "<h2>Enabled</h2>";
      }
      else
      {
        htmlOut += "<h2>Ready</h2>";
      }
    }
    else
    {
      htmlOut += "<i>self-check</i>";
    }
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>Stage</td><td>";
    htmlOut += theStatus;
    htmlOut += "</td></tr>";
    htmlOut += "<tr><th>System</th><th> </th></tr>";
    htmlOut += "<tr><td>Uptime</td><td>";
    htmlOut += String(millis() / 60000);
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>CpuFreqMHz</td><td>";
    htmlOut += (String)ESP.getCpuFreqMHz();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>FreeHeap</td><td> ";
    htmlOut += (String)ESP.getFreeHeap();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>HeapFragmentation</td><td> ";
    htmlOut += (String)ESP.getHeapFragmentation();
    htmlOut += "</td></tr>";
    //htmlOut += "<tr><td>getCycleCount</td><td> ";
    /*htmlOut += (String)ESP.getCycleCount();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>checkFlashCRC</td><td> ";
    htmlOut += (String)ESP.checkFlashCRC();
    htmlOut += "</td></tr>";*/
    htmlOut += "<tr><td>FreeSketchSpace</td><td> ";
    htmlOut += (String)ESP.getFreeSketchSpace();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>checkFlashConfig</td><td> ";
    htmlOut += (String)ESP.checkFlashConfig();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>ResetInfo</td><td> ";
    htmlOut += (String)ESP.getResetInfo();
    htmlOut += "</td></tr>";
    //WiFi.softAPDhcpServer();
    /*
    htmlOut += "<tr><td>Random</td><td> ";
    htmlOut += (String)ESP.random();
    htmlOut += "</td></tr>";
    */

    htmlOut += "<tr><th>Wifi</th><th> </th>";

    htmlOut += "<tr><td>SSID</td><td>";
    htmlOut += (String)WiFi.SSID();
    htmlOut += "</td></tr>";

     htmlOut += "<tr><td>RSSI</td><td>";
    htmlOut += (String)WiFi.RSSI();
    htmlOut += "</td></tr>";

    htmlOut += "<tr><td>Mode</td><td>";
    htmlOut += (String)WiFi.getMode();
    htmlOut += "</td></tr>";

    htmlOut += "<tr><td>Channel</td><td>";
    htmlOut += (String)WiFi.channel();
    htmlOut += "</td></tr>";
 htmlOut += "</tr>";
    htmlOut += "<tr><th>Network</th><th> </th></tr>";
    
    htmlOut += "<tr><td>host Name</td><td> ";
    htmlOut += (String)WiFi.getHostname();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>Local IP</td><td>";
    htmlOut += (String)WiFi.localIP().toString();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>Gateway IP</td><td> ";
    htmlOut += (String)WiFi.gatewayIP().toString();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>DNS IP</td><td> ";
    htmlOut += (String)WiFi.dnsIP().toString();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>Subnet</td><td>";
    htmlOut += (String)WiFi.subnetMask().toString();
    htmlOut += "</td></tr>";
    htmlOut += "<tr><td>MAC Address</td><td> ";
    htmlOut += (String)WiFi.macAddress();
    htmlOut += "</td></tr>";
    htmlOut += "</td></tr>";
    htmlOut += "<tr><th>Program</th><th> </th></tr>";
    htmlOut += "<tr><td>Settings</td><td>";
    for (int i = 0; i < (sizeof(espSettings) / sizeof(espSettings[0])); i++)
    {
      htmlOut += "<p>";
      htmlOut += espSettings[i];
      htmlOut += "</p>";
    }

    htmlOut += "</tbody></table>";
    htmlOut += getFoot();
    server.send(200, "text/html", htmlOut);
    delay(30); //rest easy
}

void handleSettings() { //can we refresh this page?
  String htmlOut = getHead();
 // htmlOut += "<meta http-equiv='refresh' content='15'>";
  htmlOut += "<table style='width:100%'><tbody>";
  htmlOut += "<tr><th><b>Control</b></th><th><b>Description</b></th></tr>";

  if(isDebugOut)
  {
    htmlOut += "<tr><th><a href='/control.html?arg=debug'>Enable Production</a></th> <th>Enable production serial output</th></tr>";  
  }
  else
  {
    htmlOut += "<tr><th><a href='/control.html?arg=debug'>Enable Debug</a></th> <th>Enable serial debugger</th></tr>";
  }

  if(apInitialized)
  {
    htmlOut += "<tr><th><a href='/control.html?arg=enableAP'>Disable AP</a></th> <th>Disable Access Point</th></tr>";  
  }
  else
  {
    htmlOut += "<tr><th><a href='/control.html?arg=enableAP'>Enable AP</a></th> <th>Enable Access Point</th></tr>";
  }

  if(autonomous)
  {
    htmlOut += "<tr><th><a href='/control.html?arg=autoMode'>Linear mode</a></th> <th>Follow along</th></hr>";
  }
  else
  {
    htmlOut += "<tr><th><a href='/control.html?arg=autoMode'>Autonomous</a></th> <th>lol</th></hr>";
  }
  //htmlOut += "<tr><th><a href='/control.html?arg=resetHeap'>Reset Heap</a></th> <th>lol</th></hr>";
  htmlOut += "<tr><th><a href='/scan.html'>WiFi</a></th></tr>";

  // ADD NEXT SETTING ABOVE : 
  htmlOut += "<tr><tr> <th></tr>";
  htmlOut += "<tr><th>";
  htmlOut += "<form action='/control.html' method='get'>";
  htmlOut += "Setting: <input type='text' name='Command:'>";
  htmlOut += "<input type='submit' value='Set'></form>";
  htmlOut += "</th><th>Format: @Setting,enable=1,time=10000,lastRun=0;</th></tr>";
  if ((WiFi.status() == WL_CONNECTED))
  {
    //htmlOut += "<tr><th><a href='/control.html?arg=update'>Get ROM update</a></th> <th>Apply ROM and settings update</th></tr>";
  }

  htmlOut += "<tr><th>";
  htmlOut += "<form action='/control.html' method = 'GET' id='carform'> Setting :<select name='cmd' form='carform'> <option value='argsAnalogRead'>analog</option> <option value='=argsDigitalRead'>digital</option> </select>Pin : <select name='Pin' form='carform'> <option value='4'>4 (SDA)</option> <option value='5'>5 (SDL)</option> <option value='12'>12 (MISO)</option><option value='13'>13 (MOSI)</option><option value='14'>14 (CLK)</option><option value='A0'>Analog 0</option></select>Enable :<select name='Enable' form='carform'> <option value='1'>True</option> <option value='0'>False</option></select>   Time:<input type='text' name='Time:'>  <input type='submit' value='Submit'></form>";
  htmlOut += "</th></tr>";
  htmlOut += "</tbody></table>";
  delay(30); //rest easy
  htmlOut += getFoot();
  server.send(200, "text/html", htmlOut);
}

void handleScanNetworks() {
  String htmlOut = getHead();

  htmlOut += "<h2>Scan Results:</h2>";
  int n = WiFi.scanNetworks();
  Out("debug handleScanNetworks scanNetworks", (String)n);

  htmlOut += "<table style='width:100%'>";
  htmlOut += "<tr><th><b>SSID</b></th><th><b>RSSI</b></th><th><b>Connect</b></th></hr>";
  
  if (n != 0)
  {
    //sort WiFi.SSID by lowest RSSI
    int iArray[100]; 
    int rssiArray[100];
    int lowestI;

    //Copy WiFi.RSSI() to rssiArray
    for (int i = 0; i < n; ++i)
    {
      rssiArray[i] = WiFi.RSSI(i); 
    }

    for (int x = 0; x <n; ++x)
    {
      int lowRSSI = -100;
      for (int i = 0; i < n; ++i) //find strongest signal in rssiArray
      {
        if (lowRSSI < rssiArray[i])
        {
          lowestI = i; 
          lowRSSI = rssiArray[i]; // Set lowRSSI equal to current, lowest, RSSI
        }
      }
      
      iArray[x] = lowestI; //Now we have the lowest RSSI ID == lowestI 
      rssiArray[lowestI] = -1000; //set current RSSI equal to -1000 so it's not checked again
      //find next lowest RSSI
    }

    // Output wifi with iteration through iArray
    // foreach(i in iArray) { Out(Wifi.SSID(i)) } 
    for(int i = 0; i < n; ++i)
    {
      if(WiFi.encryptionType(iArray[i]) == ENC_TYPE_NONE)
      {
        htmlOut += "<tr><th>"; 
        htmlOut += WiFi.SSID(iArray[i]); 
        htmlOut += "</th><th>"; 
        htmlOut += WiFi.RSSI(iArray[i]); 
        htmlOut += "</th><th>"; 
        htmlOut += "<a href='/connect.html?ssid=" + (String)WiFi.SSID(iArray[i]) + "'>Connect</a>";
        htmlOut += "</td></tr>";
      }
      else
      {
        htmlOut += "<tr><th>"; 
        htmlOut += WiFi.SSID(iArray[i]); 
        htmlOut += "</th><th>"; 
        htmlOut += WiFi.RSSI(iArray[i]); 
        htmlOut += "</th><th>"; 
        htmlOut += (String)WiFi.encryptionType(iArray[i]);
        //htmlOut += "<a href='/connect.html?ssid=" + (String)WiFi.SSID(iArray[i]) + "'>Connect</a>";
        htmlOut += "</td></tr>";
      }
    }
  }
  htmlOut += "</table><BR><BR>";

  htmlOut += "<form action='/connect.html' method='get'>";
  htmlOut += "Manual AP: <input type='text' name='ssid'>";
  htmlOut += "PSK : <input type='text' name='PSK'>";
  htmlOut += "<input type='submit' value='Connect'></form>";

  if (WiFi.status() == WL_CONNECTED)
  {
    htmlOut += "<BR><BR><a href='/control.html?arg=wifidisconnect'>Disconnect WiFi</a></th></tr>";
  }
  htmlOut += getFoot();
  delay(30); //rest easy
  server.send(200, "text/html", htmlOut); 
}

void handleConnectWifi() {
   Out("handleConnectWifi", server.arg(0));
   Out("handleConnectWifi", server.arg(1));
   WiFi.disconnect();
   WiFiMulti.cleanAPlist();
   if(server.arg(1).length() > 1)
   {
    WiFiMulti.addAP((server.arg(0)).c_str(),server.arg(1).c_str());
   }
   else
   {
    WiFiMulti.addAP((server.arg(0)).c_str());
   }
   WiFi.reconnect();
   delay(300); //rest easy
   handleRoot(); 
}
