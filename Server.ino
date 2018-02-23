#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Servo.h>

WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char incomingPacket[255];
char  replyPacekt[] = "Hi there! Got the message :-)";

const char *ssid = "nodemcu_004";
const char *password = "12345678";

Servo servo;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("");

  WiFi.mode(WIFI_AP); 
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);
  
  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", myIP.toString().c_str(), localUdpPort);  

  servo.attach(D0);  //D4
  servo.write(0);
  
  delay(2000);
}

boolean ON = true;
boolean OF = false;
boolean STATE = ON;
int OFF_ANGLE = 73;
int ON_ANGLE = 80;
short OFF_TIME = 8 * 60 * 1000;
short ON_TIME = 2 * 60 * 1000;

void loop() {
  // put your main code here, to run repeatedly:
  checkUDP();
  stayOn();
  stayOff();  
  delay(1000);
}

void stayOff(){
  setAngle(OFF_ANGLE);
  delay(OFF_TIME);
}

void stayOn(){
  setAngle(ON_ANGLE);
  delay(ON_TIME);
}

void checkUDP(){
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    Serial.printf("UDP packet contents: %s\n", incomingPacket);    
    String servoAngleStr(incomingPacket);
    int servoAngle = servoAngleStr.toInt();
    setAngle(servoAngle);
    
    // send back a reply, to the IP address and port we got the packet from
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(replyPacekt);
    Udp.endPacket();

  }
}

void setAngle(int servoAngle){
  if(servoAngle >= 0 && servoAngle <= 180){
    Serial.printf("New servo angle: %s\n", incomingPacket);
    servo.write(servoAngle);       
    delay(1000);
  }
}
