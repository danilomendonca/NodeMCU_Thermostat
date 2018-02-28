#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

//WI-FI AP
const char *ssid = "nodemcu_004";
const char *password = "u004u004";

//UDP SERVER
unsigned int localUdpPort = 4210;
WiFiUDP Udp;

//HTTP SERVER
ESP8266WebServer server(80);

//STEP MOTOR
Servo servo;

boolean CMD_MODE = true;
const boolean ON = true;
const boolean OFF = false;
boolean STATE = OFF;

struct Vote{
  String IP;
  short value;
  boolean set;
};

const short MAX_VOTES = 6;
short voteIndex = 0;
Vote votes[MAX_VOTES] = {{"", 0}, {"", 0}, {"", 0}, {"", 1}, {"", 1}, {"", 1}};

const int OFF_ANGLE = 71;
const int ON_ANGLE = 80;
const short OFF_TIME = 8 * 60 * 1000;
const short ON_TIME = 2 * 60 * 1000;
int currentAngle;

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

  setAngle(OFF_ANGLE);
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


const String coldColor = "#4232b5";
const String hotColor = "#d82f2f";
const String HtmlHtml = "<html><head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
    "<meta http-equiv=\"refresh\" content=\"60;url=/\" />"
    "</head>";
const String HtmlHtmlClose = "</html>";
const String HtmlBody = "<body style=\"text-align: center; font-family: sans-serif;\"><br/><br/>";
const String HtmlBodyClose = "</body>";
const String HtmlTitle = "<h1>DEEP-SE WEB THERMOSTAT UFFICIO P3/004</h1>\n";
const String HtmlLedStateLow = "<big>HEATER is now <b style=\"color: " + coldColor + "\">OFF</b></big><br/><br/><br/>\n";
const String HtmlLedStateHigh = "<big>HEATER is now <b style=\"color: " + hotColor + "\">ON</b></big><br/><br/><br/>\n";
const String HtmlButtons = 
    "<a href=\"fafreddo\"><button style=\"display: inline-block; height: 2em; width: 30%;\">TURN IT ON</button></a>"
    "<a href=\"facaldo\"><button style=\"display: inline-block; height: 2em; width: 30%;\">TURN IT OFF</button></a>"
    "<br/><br/><br/>";
const String HtmlColdButton = "<a href=\"fafreddo\" style=\"display: inline-block; margin: 50px;\"><img style=\"width: 100px; height: auto\" src=\"http://icons.iconarchive.com/icons/aha-soft/standard-new-year/256/Snowflake-icon.png\" alt=\"Fa Freddo\"/></a>";
const String HtmlHotButton = "<a href=\"facaldo\" style=\"display: inline-block; margin: 50px;\"><img style=\"width: 100px; height: auto\" src=\"http://icons.iconarchive.com/icons/custom-icon-design/flatastic-4/512/Hot-icon.png\" alt=\"Fa Caldo\"/></a>";

String HtmlVotes(){

  String html = "<table style=\"margin: 0 auto\">";
  for(int i = 0; i < MAX_VOTES; i++){
    Vote vote = votes[i];
    if(vote.set){
      String voteIP = "<td>IP: " + vote.IP + "</td>";
      String voteValue = "<td> voted " + getVoteFromValue(vote.value) + "</td>";
      html += "<tr>" + voteIP + voteValue;
      if(voteIndex == i)
        html += " <td><span stype=\"font-size: 1.4em;\">&larr; last vote</span></td>";
      else
        html += "<td/><td>";
      html += "<tr/>";
    }    
  }
  html += "</table>";
  return html;
}

String getVoteFromValue(short value){
  if(value)
    return "<b style=\"color: " + coldColor + "\">FA FREDDO</b>";
  else
    return "<b style=\"color: " + hotColor + "\">FA CALDO</b>";
}

String HtmlConsensus(){
  return "";
}

void response(){
  String htmlRes = HtmlHtml + HtmlBody + HtmlTitle;
  if(STATE == OFF){
    htmlRes += HtmlLedStateLow;
  }else{
    htmlRes += HtmlLedStateHigh;
  }

  htmlRes += HtmlButtons;
  //htmlRes += HtmlColdButton;
  //htmlRes += HtmlHotButton;
  htmlRes += HtmlVotes();  
  htmlRes += HtmlBodyClose;
  htmlRes += HtmlHtmlClose;

  server.send(200, "text/html", htmlRes);
}

void handleRoot(){
  response();
}

const short UP_TEMP = 1;
const short DOWN_TEMP = 0;

void handleHot(){  
  String voterIP = server.client().remoteIP().toString();
  //long timeStamp = server.arg(“timeStamp”);
  logVote(voterIP, DOWN_TEMP);  
  checkConsensus();
  response();
}

void handleCold(){  
  String voterIP = server.client().remoteIP().toString(); 
  logVote(voterIP, UP_TEMP);
  checkConsensus();
  response();
}

void handleGetState(){
  String state = STATE ? "on" : "off";
  server.send(200, "text/json", "{heater: " + state + "}");
}

void checkHTTP(){
  server.handleClient();
}

char incomingPacket[255];
const char  replyPacekt[] = "New thermostat angle received :-)";

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

void logVote(String voterIP, short voteValue){  
  Vote vote = {voterIP, voteValue, true};
  if(voteIndex + 1 < MAX_VOTES)
    voteIndex++;
  else
    voteIndex = 0;
     
  votes[voteIndex] = vote;
}

short consensus(){
  float aggregateVoteValue = 0;
  float totalVotes = 0;
  for(short i = 0; i < MAX_VOTES; i++){
    if(votes[i].set){
      totalVotes++;
      aggregateVoteValue += votes[i].value;
    }
  }
  float majorityOnValue = totalVotes / 2;
  if(aggregateVoteValue >= majorityOnValue)
    return UP_TEMP;
  else
    return DOWN_TEMP;
}

void checkConsensus(){
  if(consensus() == UP_TEMP)
    turnOn();
  else
    turnOff();
}

void turnOff(){
  changeAngle(OFF_ANGLE);
  STATE = OFF;
}

void turnOn(){
  changeAngle(ON_ANGLE);
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

const short AFTER_CHANGE_DELAY = 250;
const short MIN_ANGLE = 0;
const short MAX_ANGLE = 180;

void setAngle(int servoAngle){
  if(servoAngle >= MIN_ANGLE && servoAngle <= MAX_ANGLE){    
    servo.write(servoAngle);
    currentAngle = servoAngle;
    delay(AFTER_CHANGE_DELAY);
  }
}

void changeAngle(int targetAngle){  
  Serial.printf("Changing angle");
  Serial.printf("Current angle: %d\n", currentAngle);
  Serial.printf("Target angle: %d\n", targetAngle);
  short delta = 0;
  if(targetAngle > currentAngle)
    delta = 1;
  else if(targetAngle < currentAngle)
    delta = -1;
  while(currentAngle != targetAngle){
    currentAngle += delta;
    Serial.printf("New angle: %d\n", currentAngle);
    setAngle(currentAngle);
  }
}

