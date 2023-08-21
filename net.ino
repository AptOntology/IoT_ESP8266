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
  Serial.println("LLMNR responder started");

    // Init the (currently empty) host domain string with chipid
  MDNS.begin((String)ESP.getChipId());
  MDNS.addService("http", "tcp", 80);
  Serial.println("MDNS responder started");

  NBNS.begin(((String)ESP.getChipId()).c_str());

//ssdp
    HTTPnet.on("/", HTTP_GET, []() {
      //HTTPnet.send(200, "text/plain", "network on!");
    });
    HTTPnet.on("/description.xml", HTTP_GET, []() {
      SSDP.schema(HTTPnet.client());
    });
    HTTPnet.begin();
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(8080);
    SSDP.setName("Philips hue clone");
    SSDP.setSerialNumber("001788102201");
    SSDP.setURL("index.html");
    SSDP.setModelName("Philips hue bridge 2012");
    SSDP.setModelNumber("929000226503");
    SSDP.setModelURL("http://www.meethue.com");
    SSDP.setManufacturer("Royal Philips Electronics");
    SSDP.setManufacturerURL("http://www.philips.com");
    SSDP.begin();
//end ssdp
}

void loopNet()
{
  HTTPnet.handleClient();
  MDNS.update();
}
