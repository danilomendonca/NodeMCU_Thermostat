#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

const char *ssid = "nodemcu_004";
const char *password = "u004u004";

WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char incomingPacket[255];
char  replyPacekt[] = "Hi there! Got the message :-)";

ESP8266WebServer server(80);

Servo servo;

boolean CMD_MODE = true;
const boolean ON = true;
const boolean OFF = false;
boolean STATE = OFF;

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

  server.on("/", handleRoot);
  server.on("/heater", handleGetState);
  server.on("/fafreddo", handleCold);
  server.on("/facaldo", handleHot);  
  server.begin();
  Serial.println("HTTP server beginned");

  servo.attach(D0);  //D4
  turnOff();
  
  delay(2000);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(CMD_MODE){
    checkUDP(); //check for UDP messages
    checkHTTP(); //check for HTTP requests
  }else{
    stayOn(); //stays on for 2 min
    stayOff(); //stays off for 8 min
  }
  delay(1000);
}

const String HtmlHtml = "<html><head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
    "<meta http-equiv=\"refresh\" content=\"30;url=/\" />"
    "</head>";
const String HtmlHtmlClose = "</html>";
const String HtmlBody = "<body style=\"text-align: center; font-family: sans-serif;\"><br/><br/>";
const String HtmlBodyClose = "</body>";
const String HtmlTitle = "<h1>DEEP-SE WEB THERMOSTAT UFFICIO P3/004</h1>\n";
const String HtmlLedStateLow = "<big>HEATER is now <b style=\"color:blue\">OFF</b></big><br/><br/><br/>\n";
const String HtmlLedStateHigh = "<big>HEATER is now <b style=\"color:red\">ON</b></big><br/><br/><br/>\n";
const String HtmlButtons = 
    "<a href=\"fafreddo\"><button style=\"display: inline-block; width: 40%;\">TURN IT ON</button></a>"
    "<a href=\"facaldo\"><button style=\"display: inline-block; width: 40%;\">TURN IT OFF</button></a>";

void response(){
  String htmlRes = HtmlHtml + HtmlBody + HtmlTitle;
  if(STATE == OFF){
    htmlRes += HtmlLedStateLow;
  }else{
    htmlRes += HtmlLedStateHigh;
  }

  htmlRes += HtmlButtons;
  htmlRes += HtmlBodyClose;
  htmlRes += HtmlHtmlClose;

  server.send(200, "text/html", htmlRes);
}

void handleRoot(){
  response();
}

void handleHot(){
  turnOff();
  response();
}

void handleCold(){
  turnOn();
  response(); 
}

void handleGetState(){
  String state = STATE ? "on" : "off";
  server.send(200, "text/json", "{heater: " + state + "}");
}

int OFF_ANGLE = 73;
int ON_ANGLE = 80;
short OFF_TIME = 8 * 60 * 1000;
short ON_TIME = 2 * 60 * 1000;

void turnOff(){
  setAngle(OFF_ANGLE);
  STATE = OFF;
}

void turnOn(){
  setAngle(ON_ANGLE);
  STATE = ON;
}

void stayOff(){
  turnOff();
  delay(OFF_TIME);
}

void stayOn(){
  turnOn();
  delay(ON_TIME);
}

void checkHTTP(){
  server.handleClient();
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
