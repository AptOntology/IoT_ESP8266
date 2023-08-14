void handleRoot();
void handleSettings();
void handleInfo();
void handleScanNetworks();
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

bool apInitialized;

void SetupAP() {
  if(apInitialized == false)
  {
    pinMode(A0, INPUT);
    
    apInitialized = true;
    if(enableAP)
    {
      WiFi.mode(WIFI_AP_STA);
      WiFi.disconnect();
      delay(50);
      WiFi.softAP(("ESP_" + (String)(ESP.getChipId())).c_str(), "");
    }
    // Web server
    server.on("/", handleRoot);
    server.on("/scan.html", handleScanNetworks);
    server.on("/connect.html", handleConnectWifi); 
    server.on("/control.html", handleControl); 
    server.on("/settings.html", handleSettings);
    server.on("/info.html", handleInfo);
    server.begin();
    Out("SetupAP", "Complete");
  }
}

void LoopAP()
{
  SetupAP();
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
  String htmlHead1 = "<html> <head> <style> .flex-container { display: -webkit-flex; display: flex; -webkit-flex-flow: row wrap; flex-flow: row wrap; text-align: center; } .flex-container > * { padding: 15px; -webkit-flex: 1 100%; flex: 1 100%; } .article { text-align: left; } header {background: black;color:white;} footer {background: #aaa;color:white;} .nav {background:#eee;} .nav ul { list-style-type: none; padding: 0; } .nav ul a { text-decoration: none; } @media all and (min-width: 768px) { .nav {text-align:left;-webkit-flex: 1 auto;flex:1 auto;-webkit-order:1;order:1;} .article {-webkit-flex:5 0px;flex:5 0px;-webkit-order:2;order:2;} footer {-webkit-order:3;order:3;} } </style> </head> <body> <div class='flex-container'> <header> <h1>ESP_";
  String htmlHead2 = "</h1> </header> <nav class='nav'> <ul> ";
  htmlHead2 += "<li><h2><a href='/'>Root</a></h2></li> "; 
  htmlHead2 += "<li><h2><a href='/info.html'>Info</a></h2></li> "; 
  htmlHead2 += "<li><h2><a href='/scan.html'>Wifi</a></h2></li> ";
  htmlHead2 += "<li><h2><a href='/settings.html'>Settings</a></h2></li> "; 
  String htmlHead3 = "</ul> </nav> <article class='article'>";
  String htmlHead = htmlHead1 + (String)(ESP.getChipId()) + htmlHead2 + htmlHead3;
  return htmlHead;
}

String getFoot()
{
  String htmlFoot = "</article> <footer> Have a nice day </footer></div></body></html>";
  return htmlFoot;
}

void handleRoot() { 
  String htmlOut = getHead();
  if (WiFi.status() == WL_CONNECTED)
  {
    htmlOut += "<h2>SSID: ";
    htmlOut += WiFi.SSID();
    htmlOut += "</h2>";
    htmlOut += "<h2>RSSI: ";
    htmlOut += WiFi.RSSI();
    htmlOut += "</h2>";
  }
  htmlOut += "<h2>A0: "; 
  htmlOut += (String)analogRead(A0);
  htmlOut += " </h2>";
  htmlOut += "<h2>Uptime: "; 
  htmlOut += String(millis() / 60000);
  htmlOut += " minutes</h2>";
  htmlOut += getFoot();
  server.send(200, "text/html", htmlOut);
  delay(30); //rest easy
}

void handleControl()
{
  if(server.arg(0).indexOf("enableAP") >= 0) //
  {
    if(enableAP)
    {
      Out("debug handleControl enableAP", "false");
      enableAP = false;
      WiFi.mode(WIFI_STA);
    }
    else
    {
       Out("debug handleControl enableAP", "true");
      apInitialized = false;
      enableAP = true;
    }
  }
  if(server.arg(0) == "5")
  {
    Out("handleControl","It's five!");
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
      UpdateSettings("@debug,enable=1,false;");
    }
    else
    {
      UpdateSettings("@debug,enable=1,true;");
    }
  }
  
  handleRoot();
}

void handleInfo()
{
    String htmlOut = getHead();
    htmlOut += "<h2>CpuFreqMHz: ";
    htmlOut += (String)ESP.getCpuFreqMHz();
    htmlOut += "</h2>";
        htmlOut += "<h2>FreeHeap: ";
    htmlOut += (String)ESP.getFreeHeap();
    htmlOut += "</h2>";
        htmlOut += "<h2>HeapFragmentation: ";
    htmlOut += (String)ESP.getHeapFragmentation();
    htmlOut += "</h2>";

            htmlOut += "<h2>FlashChipId: ";
    htmlOut += (String)ESP.getFlashChipId();
    htmlOut += "</h2>";
            htmlOut += "<h2>getCycleCount: ";
    htmlOut += (String)ESP.getCycleCount();
    htmlOut += "</h2>";
            htmlOut += "<h2>checkFlashCRC: ";
    htmlOut += (String)ESP.checkFlashCRC();
    htmlOut += "</h2>";
                htmlOut += "<h2>checkFlashConfig: ";
    htmlOut += (String)ESP.checkFlashConfig();
    htmlOut += "</h2>";
                htmlOut += "<h2>macAddress: ";
    htmlOut += (String)WiFi.macAddress();
    htmlOut += "</h2>";
                htmlOut += "<h2>hostName: ";
    htmlOut += (String)WiFi.getHostname();
    htmlOut += "</h2>";

            htmlOut += "<h2>Random: ";
    htmlOut += (String)ESP.random();
    htmlOut += "</h2>";

    htmlOut += getFoot();
    server.send(200, "text/html", htmlOut);
    delay(30); //rest easy
}

void handleSettings() {
  String htmlOut = getHead();
  htmlOut += "<table style='width:100%'><tbody>";
  htmlOut += "<tr><th><b>Control</b></th><th><b>Description</b></th></tr>";
  if(isDebugOut)
  {
    htmlOut += "<tr><th><a href='/control.html?arg=debug'>Production</a></th> <th>Enable production serial output</th></tr>";  
  }
  else
  {
    htmlOut += "<tr><th><a href='/control.html?arg=debug'>Debug</a></th> <th>Enable serial debugger</th></tr>";
  }

  if(enableAP)
  {
    htmlOut += "<tr><th><a href='/control.html?arg=enableAP'>Disable AP</a></th> <th>Disable Access Point</th></tr>";  
  }
  else
  {
    htmlOut += "<tr><th><a href='/control.html?arg=enableAP'>Enable AP</a></th> <th>Enable Access Point</th></tr>";
  }

  htmlOut += "<tr><th>";

  htmlOut += "<form action='/control.html' method='get'>";
  htmlOut += "Setting: <input type='text' name='setting'>";
  htmlOut += "<input type='submit' value='control'>";
  htmlOut += "</th><th>@Setting,enable=1,time=1000,lastRun;</th></tr>";
  if ((WiFi.status() == WL_CONNECTED))
  {
    //htmlOut += "<tr><th><a href='/control.html?arg=update'>Get ROM update</a></th> <th>Apply ROM and settings update</th></tr>";
  }
  htmlOut += "</tbody></table>";
  htmlOut += "<p></p>";
  htmlOut += "<table style='width:100%'><tbody>";
  //htmlOut += "<tr><th><b></b></th><th><b>Value</b></th></tr>";
  // htmlOut += "<tr><th>Setting : </th><th><select name='SETTING' form='carform'> <option value='analog'>analog</option> <option value='digital'>digital</option> <option value='dht11'>dht11</option> <option value='dht22'>dht22</option> <option value='dht22'>gps</option></select> </th></tr><tr><th>PIN : </th><th><select name='PIN' form='carform'> <option value='4'>4 (SDA)</option> <option value='5'>5 (SDL)</option> <option value='12'>12 (MISO)</option> <option value='13'>13 (MOSI)</option> <option value='14'>14 (CLK)</option> <option value='A0'>Analog 0</option></select> </th></tr><tr><th>Enable :</th><th><select name='ENABLE' form='carform'> <option value='true'>True</option> <option value='false'>False</option></select></th></tr><tr><th><form action='/control.html' id='carform' method = 'GET' oninput='x.value=parseInt(THRESHOLD.value)/parseInt(60000)'> Threshold : <input type='range' min='1000' max='600000' id='THRESHOLD' name='THRESHOLD' value='120000' form='carform'> minutes : <output name='x' for='THRESHOLD'></output> </th></tr><tr><th></th><th> <input type='submit'></form></th></tr>";
  
  htmlOut += "</tbody></table>";

  delay(30); //rest easy
  htmlOut += getFoot();
  server.send(200, "text/html", htmlOut);
}

void handleScanNetworks() {
  String htmlOut = getHead();

  htmlOut += "<h2>Scan Results:</h2>";
  int n = WiFi.scanNetworks();
  
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
        //htmlOut += "</th><th>"; 
        //htmlOut += WiFi.encryptionType(iArray[i]);
        htmlOut += "</th><th>"; 
        htmlOut += "<a href='/connect.html?ssid=" + (String)WiFi.SSID(iArray[i]) + "'>Connect</a>";
        htmlOut += "</th></tr>";
      }
    }
  }
  htmlOut += "</table><BR><BR>";

  htmlOut += "<form action='/connect.html' method='get'>";
  htmlOut += "Manual AP: <input type='text' name='ssid'>";
  htmlOut += "<input type='submit' value='Connect'>";

  if (WiFi.status() == WL_CONNECTED)
  {
    //htmlOut += "<BR><BR><a href='/control.html?arg=2'>Disconnect WiFi</a></th></tr>";
  }
  htmlOut += getFoot();
  delay(30); //rest easy
  server.send(200, "text/html", htmlOut); 
}

void handleConnectWifi() {
   Out("handleConnectWifi", server.arg(0));

   WiFi.disconnect();
   WiFiMulti.addAP((server.arg(0)).c_str());
   //ConnectWifi(server.arg(0),"");
   setup();
   delay(300); //rest easy
   handleRoot(); 
}
