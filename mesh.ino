WiFiServer meshServer(8080);

bool isMeshSetup;

void SetupMesh() {
  if(isMeshSetup == false) {
    SetupAP();
    meshServer.begin();
    Out("SetupMesh","begin");
    isMeshSetup = true;
  }
}

void LoopMesh() {
  SetupMesh();
  // Check if a client has connected
  WiFiClient client = meshServer.available();
  if (!client) {
    return;
  }
  
  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available()){
    delay(1);
  }

  while(client.available())
  {
    delay(1);
    while(client.connected())
    {
      if(client.available())
      {
        String req = client.readString();
        Serial.println(req);
        if(req.indexOf("SETTING=") != -1 || req.indexOf("ACTION=") != -1)
          UpdateSetting(req);
        
        if(req.indexOf("RELAY=") != -1)
          if(MeshRelay(req))
            client.println("SUCCESS");
          else
            client.println("FAIL");
        
        delay(10);
      }
      else
      {
        Out("", "Client end");
        break;
      }
    }
    
    client.flush();
  
    delay(1000);
    Serial.println("Client disonnected");
  }
}

bool MeshRelay(String line)
{
    // line == RELAY=sensornet/1234567/Humidity,10
  if(line.indexOf("RELAY=") != -1)
    line = line.substring(line.indexOf("RELAY=")+1, line.length()); //remove RELAY=

  String mqttPublishStr = line.substring(0, line.indexOf(","));
  String sensorValue = line.substring(line.indexOf(",") + 1, line.length());
  if (WiFi.status() != WL_CONNECTED)
  {
    if(MqttConnect())
    {
        MqttPublish(mqttPublishStr, (String)sensorValue);
    }
    else
    {
      //Connected to wifi and not MQTT. If connected to ESP node then relay packet 
      if(WiFi.SSID().indexOf("ESP_") != -1)
      {
        MeshClient(line);
      }
      else
      {
        return false;
      }
    }
  }
  else
  {
    //We're not connected, send response back to client
    return false;
  }
  return true; 
}

void MeshClient(String line)
{
  //connect to port on server
  //MeshClient should be utilized instead of PublishMqtt when offline 
}
