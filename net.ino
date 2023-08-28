/*
ESP network services

Todo 
network protocol support
ssdp,llmnr,mdns,scan
*/
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <ESP8266LLMNR.h>
#include <ESP8266mDNS.h>
#include <ESP8266NetBIOS.h>

ESP8266WebServer HTTPnet(8080); //ssdp

void setupNet()
{
  LLMNR.begin(((String)ESP.getChipId()).c_str());
  Out("LLMNR responder",(String)LLMNR.status());

  MDNS.end();
  MDNS.begin((String)WiFi.getHostname());
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "/", "http://"+(String)WiFi.localIP().toString());
  Serial.println("MDNS responder started");

  NBNS.begin(((String)WiFi.getHostname()).c_str());
  NBNS.
  Serial.println("NBNS responder started");

//ssdp
    HTTPnet.on("/", HTTP_GET, []() {
      HTTPnet.send(200, "text/plain", "on!");
    });
    HTTPnet.on("/description.xml", HTTP_GET, []() {
      SSDP.schema(HTTPnet.client());
    });
    HTTPnet.begin();
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(8080);
    SSDP.setName("ESP8266 client");
    SSDP.setSerialNumber("001788102201");
    SSDP.setURL("/");
    SSDP.setModelName("ESP8266");
    SSDP.setModelNumber("929000226503");
    SSDP.setModelURL("http://www.myURL");
    SSDP.setManufacturer("Espressif");
    SSDP.setManufacturerURL("http://www.espressif.com");
    SSDP.begin();
//end ssdp
}

void loopNet()
{
  HTTPnet.handleClient();
  MDNS.update();
}
