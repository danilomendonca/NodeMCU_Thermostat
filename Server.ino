#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

#include "idDHTLib.h"

//DHT Sensor
#define DHTPIN D4     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
//DHT dht(DHTPIN, DHTTYPE);
idDHTLib dht(DHTPIN, idDHTLib::DHT22);

//WI-FI AP
//const char *ssid = "nodemcu_004";
const char *ssid = "Ufficio_004";
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

const long MAX_VOTE_AGE = 1 * 60 * 60 * 1000;

struct Vote{
  String IP;
  short value;
  long timeStamp;
  boolean set;
  String name;
  long getVoteAge() const{
    return millis() - timeStamp;
  }
  short getVoteAgeInMinutes() const{
    return getVoteAge() / (1000 * 60);
  }
  boolean isExpired(){
    return getVoteAge() > MAX_VOTE_AGE;
  }
};

const short MAX_VOTES = 8;
short voteIndex = 0;
Vote votes[MAX_VOTES] = {{"", 0, 0}, {"", 0, 0}, {"", 0, 0}, {"", 0, 0}, {"", 1, 0}, {"", 1, 0}, {"", 1, 0}, {"", 1, 0}};

struct LocalClimate{
  float temperatureCelsius;
  float humidity;
  float heatIndex;
  int timeSinceLastRead;
};

LocalClimate officeClimate = {0, 0, 0};

const int OFF_ANGLE = 70;
const int ON_ANGLE = 77;
const short OFF_TIME = 8 * 60 * 1000;
const short ON_TIME = 2 * 60 * 1000;
int currentAngle;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while(!Serial) { }

  //WiFi.mode(WIFI_AP); 
  //WiFi.softAP(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { //Wait for connection
    delay(500);
    Serial.println("Waiting to connect…");
  }

  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);
  
  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", myIP.toString().c_str(), localUdpPort);  

  server.on("/", handleRoot);
  server.on("/heater", handleGetState);
  server.on("/climate", handleGetClimate);
  server.on("/fafreddo", handleCold);
  server.on("/facaldo", handleHot);  
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent","Accept"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
  Serial.println("HTTP server beginned");

  servo.attach(D0);  //D4  

  setAngle(OFF_ANGLE);
  delay(2000);
  
}

const int LOOP_INTERVAL = 1000;

void loop() {

  readTemperature();
  
  if(CMD_MODE){
    checkUDP(); //check for UDP messages
    checkHTTP(); //check for HTTP requests
  }else{
    stayOn(); //stays on for 2 min
    stayOff(); //stays off for 8 min
  }
  delay(LOOP_INTERVAL);
  officeClimate.timeSinceLastRead += LOOP_INTERVAL;
}

const short READ_INTERVAL = 5000;

void readTemperature(){
  // Report every 2 seconds
  if(officeClimate.timeSinceLastRead > READ_INTERVAL) {
    int result = dht.acquireAndWait();
    if (result == IDDHTLIB_OK) {
      // Reading temperature or humidity takes about 250 milliseconds!
      // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
      //float h = dht.readHumidity();
      float h = dht.getHumidity();
      // Read temperature as Celsius (the default)
      //float t = dht.readTemperature();
      float t = dht.getCelsius();
      
      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        officeClimate.timeSinceLastRead = 0;
        return;
      }
  
      // Compute heat index in Celsius (isFahreheit = false)
      //float hic = dht.computeHeatIndex(t, h, false);
      float hic = dht.getDewPoint();
  
      officeClimate.humidity = h;
      officeClimate.temperatureCelsius = t;
      officeClimate.heatIndex = hic;
    }else{
      Serial.println("Failed to read from DHT sensor!");
    }
  }
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
    "<a href=\"fafreddo\" onclick=\"appendNameAndNavigate(this.href); return false;\"><button style=\"display: inline-block; height: 2em; width: 30%;\">TURN IT ON</button></a>"
    "<a href=\"facaldo\" onclick=\"appendNameAndNavigate(this.href); return false;\"><button style=\"display: inline-block; height: 2em; width: 30%;\">TURN IT OFF</button></a>"
    "<br/><br/><br/>";
const String HtmlColdButton = "<a href=\"fafreddo\" style=\"display: inline-block; margin: 50px;\"><img style=\"width: 100px; height: auto\" src=\"http://icons.iconarchive.com/icons/aha-soft/standard-new-year/256/Snowflake-icon.png\" alt=\"Fa Freddo\"/></a>";
const String HtmlHotButton = "<a href=\"facaldo\" style=\"display: inline-block; margin: 50px;\"><img style=\"width: 100px; height: auto\" src=\"http://icons.iconarchive.com/icons/custom-icon-design/flatastic-4/512/Hot-icon.png\" alt=\"Fa Caldo\"/></a>";

String PageJSScript(){

  String jsScript = "<script>"  
  "let personName = \"\";"  
  "function getName(){"
    "if (typeof(Storage) !== \"undefined\" && localStorage.personName !== \"undefined\"){"
        "personName = localStorage.personName;"
    "}"
    "if (personName == null || personName == \"\") {"
        "personName = prompt(\"Please enter your name\", \"\");"
    "}"    
  "}"
  "function persistName(){"
    "if (personName != null && personName != \"\") {"
        "localStorage.personName = personName;"        
    "}"    
  "}"
  "function appendNameAndNavigate(url){"
    "getName();"
    "persistName();"
    "if (url != null && personName != null && personName != \"\") {"
        "url += \"?name=\" + personName;"
    "}"
    "asyncRequest(url);"    
    "window.location.href = '/';"
  "}"
  "function asyncRequest(url){"
    "xhttp.open(\"GET\", url, true);"
    "xhttp.send();"
  "}"
  "</script>";
  return jsScript;
}

String HtmlVotes(){

  String html = "<table style=\"margin: 0 auto\">";
  for(int i = 0; i < MAX_VOTES; i++){
    Vote vote = votes[i];
    if(vote.set){
      html += "<tr " + getVoteStyle(vote.isExpired()) + ">"; 
      String voteIP = "<td>IP: " + vote.IP + " " + HtmlVoteName(vote.name) + "</td>";
      String voteValue = "<td> voted " + HtmlVoteValue(vote.value) + "</td>";      
      String voteAge = "<td> " + String(vote.getVoteAgeInMinutes()) + " minutes ago</td>";
      html += voteIP + voteValue + voteAge;   
      html += "<tr/>";
    }    
  }
  html += "</table><br/><br/>";
  return html;
}

String getVoteStyle(boolean expired){
  if(expired)
    return "style=\"text-decoration: line-through;\"";
  else
    return "";
}

String HtmlVoteValue(short value){
  if(value)
    return "<b style=\"color: " + coldColor + "\">FA FREDDO</b>";
  else
    return "<b style=\"color: " + hotColor + "\">FA CALDO</b>";
}

String HtmlVoteName(String name){
  if(name.length() > 0)
    return "(" + name + ")";
  else
    return "";
}

String HtmlClimate(){

  String html = "<table style=\"margin: 0 auto\">";
  html += "<tr><td>Current Temperature:</td><td>" + String(officeClimate.temperatureCelsius) + "</td></tr>";
  html += "<tr><td>Current Humidity:</td><td>" + String(officeClimate.humidity) + "</td></tr>";
  html += "<tr><td>Current Heat Index:</td><td>" + String(officeClimate.heatIndex) + "</td></tr>";
  html += "</table>";
  return html;
}

String HtmlConsensus(){
  return "";
}

void responseHTML(){
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
  htmlRes += HtmlClimate();
  htmlRes += PageJSScript();
  htmlRes += HtmlHtmlClose;

  server.send(200, "text/html", htmlRes);
}

void handleRoot(){
  responseHTML();
}

const short UP_TEMP = 1;
const short DOWN_TEMP = 0;

const String HTML_REQUEST = "text/html";
const String JSON_REQUEST = "application/json";

String getRequestType(){  
  //Serial.printf("Accept: %s", server.header("Accept").c_str());  
  if(server.header("Accept").indexOf(HTML_REQUEST) >= 0)
    return HTML_REQUEST;
  else
    return JSON_REQUEST;
}

boolean validateRequest(){
  String voterIP = server.client().remoteIP().toString();
  if(voterIP.equals("") || voterIP.equals("0.0.0.0"))
    return false;  
  else
    return true;
}

void handleHot(){
  if(validateRequest()){
    String voterIP = server.client().remoteIP().toString();
    String name = getArgName();
    logVote(voterIP, DOWN_TEMP, name);
    checkConsensus();
    if(getRequestType() == HTML_REQUEST)
      responseHTML();
    else
      server.send(200, "text/json", "You voted 'fa caldo'. Viva la democrazia!");
  }else
    responseHTML();
}

void handleCold(){  
  if(validateRequest()){
    String voterIP = server.client().remoteIP().toString(); 
    String name = getArgName();
    logVote(voterIP, UP_TEMP, name);
    checkConsensus();
    if(getRequestType() == HTML_REQUEST)
      responseHTML();
    else
      server.send(200, "text/json", "You voted 'fa freddo'. Viva la democrazia!");
  }else
    responseHTML();
}

String getArgName(){
  if(server.arg("name") != "")
    return server.arg("name");
  else
    return "";
}

void handleGetState(){
  String state = STATE ? "on" : "off";
  server.send(200, "text/json", "{heater: " + state + "}");
}

void handleGetClimate(){
  String temperatureCelsiusString = String(officeClimate.temperatureCelsius);
  String humidityString = String(officeClimate.humidity);
  String heatIndexString = String(officeClimate.heatIndex);
  String response = "{"
                      "{temperatureCelsius: " + temperatureCelsiusString + "}"
                      "{humidity: " + humidityString + "}"
                      "{heatIndex: " + heatIndexString + "}"
                    "}";
  
  server.send(200, "text/json", response);
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

void rotateVotes(){  
  for(short i = 1; i < MAX_VOTES; i++){    
    votes[i - 1] = votes[i];
  }
}

short findPreviousVote(String voterIP){
  for(short i = 0; i < MAX_VOTES; i++){
    if(votes[i].IP.equals(voterIP))
      return i;
  }
  return -1;
}

void logVote(String voterIP, short voteValue, String name){    
  Vote vote = {voterIP, voteValue, millis(), true, name};
  short previousVoteIndex = findPreviousVote(voterIP);
  short currentVoteIndex = MAX_VOTES - 1;
  if(previousVoteIndex == -1)
    rotateVotes();    
  else
    currentVoteIndex = previousVoteIndex;
  votes[currentVoteIndex] = vote;  
}

short consensus(){
  float aggregateVoteValue = 0;
  float totalVotes = 0;
  for(short i = 0; i < MAX_VOTES; i++){
    if(votes[i].set && votes[i].isExpired()){
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

const short AFTER_CHANGE_DELAY = 150;
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

