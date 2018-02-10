void handleRoot();
void handleSettings();
void handleScanNetworks();
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

bool apInitialized;

void SetupAP()
{
  if(apInitialized == false)
  {
    apInitialized = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    
      // Wifi Station 
    String ssidStr = (String)"ESP_" + (String)(chipID);
    Out("SetupAP", "SSID: " + ssidStr);
    WiFi.softAP(ssidStr.c_str(), "");
  
    // Web server
    //server.on("/", handleRoot);
    //server.on("/scan.html", handleScanNetworks);
    //server.on("/connect.html", handleConnectWifi); 
    //server.on("/control.html", handleControl); 
    //server.on("/settings.html", handleSettings);
    server.begin();
    Out("SetupAP", "Initialized");
  }
}

void LoopAP()
{
  server.handleClient();
}
/*
String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String htmlHead1 = "<html> <head> <style> .flex-container { display: -webkit-flex; display: flex; -webkit-flex-flow: row wrap; flex-flow: row wrap; text-align: center; } .flex-container > * { padding: 15px; -webkit-flex: 1 100%; flex: 1 100%; } .article { text-align: left; } header {background: black;color:white;} footer {background: #aaa;color:white;} .nav {background:#eee;} .nav ul { list-style-type: none; padding: 0; } .nav ul a { text-decoration: none; } @media all and (min-width: 768px) { .nav {text-align:left;-webkit-flex: 1 auto;flex:1 auto;-webkit-order:1;order:1;} .article {-webkit-flex:5 0px;flex:5 0px;-webkit-order:2;order:2;} footer {-webkit-order:3;order:3;} } </style> </head> <body> <div class='flex-container'> <header> <h1>ESP_";
String htmlHead2 = "</h1> </header> <nav class='nav'> <ul> ";
String htmlHead3 = "<li><h2><a href='/'>Root</a></h2></li> "; 
String htmlHead4 = "<li><h2><a href='/scan.html'>Wifi</a></h2></li> ";
String htmlHead5 = "<li><h2><a href='/settings.html'>Settings</a></h2></li> "; 
String htmlHeadEnd = "</ul> </nav> <article class='article'>";
String htmlHead = htmlHead1 + (String)(chipID) + htmlHead2 + htmlHead3 + htmlHead4 + htmlHead5 +  htmlHeadEnd;
String htmlFoot = "</article> <footer> Have a nice day </footer></div></body></html>";

void handleRoot() { 
  String htmlOut = htmlHead;

  if (WiFi.status() == WL_CONNECTED)
  {
    htmlOut += "<h2>SSID: ";
    htmlOut += WiFi.SSID();
    htmlOut += "</h2>";
    htmlOut += "<h2>RSSI: ";
    htmlOut += WiFi.RSSI();
    htmlOut += "</h2>";
  }
  htmlOut += "<h2>Uptime: "; 
  htmlOut += (millis() / 60000);
  htmlOut += " minutes</h2>";

  htmlOut += htmlFoot;
  
  server.send(200, "text/html", htmlOut);
  delay(30); //rest easy
}

void handleControl()
{
  if(server.arg(0) == "5")
  {
    GetUpdate();
  }
}

void handleSettings()
{/*
  String htmlOut = htmlHead;
  htmlOut += "<table style='width:100%'>";
  htmlOut += "<tr><th><b>Control</b></th><th><b>Description</b></th>";
  
  htmlOut += "";
  if(true)
  {
    //htmlOut += "<tr><th><a href='/control.html?arg=disablepublishanalog'>Disable Publish Analog Pin</a></th> <th></th></tr>";
  }
  else
  {
   // htmlOut += "<tr><th><a href='/control.html?arg=enablepublishanalog'>Enable Publish Analog Pin</a></th> <th></th></tr>";      
  }
  if(true)
  {
   // htmlOut += "<tr><th><a href='/control.html?arg=disablepublishDHT'>Disable Publish Temp/Humidity</a></th> <th></th></tr>";
  }
  else
  {
    //htmlOut += "<tr><th><a href='/control.html?arg=enablepublishDHT'>Enable Publish Temp/Humidity</a></th> <th></th></tr>";      
  }
  if ((WiFi.status() == WL_CONNECTED))
  {
    htmlOut += "<tr><th><a href='/control.html?arg=update'>Get ROM update</a></th> <th>Apply ROM and settings update</th></tr>";
  }
  
  htmlOut += "</table>";
  
  delay(30); //rest easy
  server.send(200, "text/html", htmlOut);*/
//}

void handleScanNetworks()
{/*
  String htmlOut = htmlHead;

  htmlOut += "<h2>Scan Results:</h2>";
  int n = WiFi.scanNetworks();
  
  htmlOut += "<table style='width:100%'>";
  htmlOut += "<tr><th><b>SSID</b></th><th><b>RSSI</b></th><th><b>Connect</b></th></hr>";
  
  if (n == 0)
  {
          //Out("ScanWifi", "No networks found");
  }
  else
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
      if(WiFi.encryptionType(iArray[i]) == ENC_TYPE_NONE && ConfirmOfflineWifi(WiFi.SSID(iArray[i]).c_str()) == false)
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
  htmlOut += htmlFoot;
  delay(30); //rest easy
  server.send(200, "text/html", htmlOut); */
}

void handleConnectWifi()
{
   Out("handleConnectWifi", server.arg(0));

   WiFi.disconnect();
   ConnectWifi(server.arg(0),"");
   delay(300); //rest easy
   handleRoot(); 
}
