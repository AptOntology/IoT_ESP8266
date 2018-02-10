//the time when the sensor outputs a low impulse
long unsigned int lowTime;       
long unsigned int highTime;   
long unsigned int lowTime2;       
long unsigned int highTime2;     

//the amount of milliseconds the sensor has to be low 
//before we assume all motion has stopped
long unsigned int pause1 = 3000;  
boolean lockLow = true;
boolean takeLowTime;
boolean lockLow2 = true;
boolean takeLowTime2;

void PulseStart(int pinNumber, bool motionOnHigh) {
  Out("PulseStart", (String)pinNumber);
  
  pinMode(pinNumber, INPUT);
  if(motionOnHigh)
    digitalWrite(pinNumber, LOW);
  else
    digitalWrite(pinNumber, HIGH);
}

void DigitalPulse(int pinNumber, bool motionOnHigh) {
  if(motionOnHigh)
  {
    if(digitalRead(pinNumber) == HIGH)
    {
      //pir is HIGH on and LOW off
       if(lockLow)
       {  
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow = false;     
         highTime = millis();       
         MqttPublish(("sensornet/" + (String)(chipID) + ("/signalhigh/" + (String)pinNumber)), (String)true);
         delay(50);
       }         
       takeLowTime = true;
    }
    if(digitalRead(pinNumber) == LOW)
    {
      if(takeLowTime)
      {
      lowTime = millis();          //save the time of the transition from high to LOW
      takeLowTime = false;       //make sure this is only done at the start of a LOW phase
      highTime = 0;
      }
      //if the sensor is low for more than the given pause, assume that no more motion is going to happen
      if(!lockLow && millis() - lowTime > pause1)
      {
         //makes sure this block of code is only executed again after a new motion sequence has been detected
         lockLow = true;                        
         MqttPublish(("sensornet/" + (String)(chipID) + ( "/signalhigh/" + (String)pinNumber)), (String)false);
         delay(50);
      }
    }
  }
  else
  {
    //infrared is HIGH off and LOW on
    if(digitalRead(pinNumber) == LOW)
    {
      //pir is HIGH on and LOW off
       if(lockLow2)
       {  
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow2 = false;     
         highTime2 = millis();       
         MqttPublish(("sensornet/" + (String)(chipID) + ( "/signallow/" + (String)pinNumber)), (String)false);
         delay(50);
       }         
       takeLowTime2 = true;
    }
    if(digitalRead(pinNumber) == HIGH)
    {
      if(takeLowTime2)
      {
        lowTime2 = millis();          //save the time of the transition from high to LOW
        takeLowTime2 = false;       //make sure this is only done at the start of a LOW phase
        highTime2 = 0;
      }
      //if the sensor is low for more than the given pause, assume that no more motion is going to happen
      if(!lockLow2 && millis() - lowTime2 > pause1)
      {
         //makes sure this block of code is only executed again after a new motion sequence has been detected
         lockLow2 = true;                        
         MqttPublish(("sensornet/" + (String)(chipID) + ( "/signallow/" + (String)pinNumber)), (String)true);
         delay(50);
      }
    }
  }
}

