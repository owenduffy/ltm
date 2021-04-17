
//Logging themperature meter for ESP8266 - NodeMCU 1.0 (ESP-12E Module)
//Copyright: Owen Duffy    2021/04/14
//I2C LCD

#define RESULTL 50
extern "C" {
//#include "user_interface.h"
}
#include <FS.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <LcdBarGraphX.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <DNSServer.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <PageBuilder.h>
#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer  server;
#elif defined(ARDUINO_ARCH_ESP32)
WebServer  server;
#endif
#include <WiFiManager.h>
#include <ArduinoJson.h>

const char ver[]="0.01";
char hostname[11]="ltm01";
int t=0;
byte lcdNumCols=16;
int sensorPin=A0; // select the input pin for the thermistor
unsigned AdcAccumulator; // variable to accumulate the value coming from the sensor
float vin;
float rt;
int avg=3;
char unit[9]="";
int lcdfsd=0;
float vref=1.0,vcc=3.3,r13=2.2e5,r14=1e5,toffset=0.0,dissfact=1e6;
float rs,sha,shb,shc,temperature;
char name[21];
char result1[RESULTL][21]; //timestamp array
float result2[RESULTL]; //temperature array
int resulti,resultn;
int i,j;
bool tick1Occured,timeset;
const int timeZone=0;
static const char ntpServerName[]="pool.ntp.org";
LiquidCrystal_I2C lcd(0x20,4,5,6,0,1,2,3);  //set the LCD I2C address
LcdBarGraphX lbg(&lcd,lcdNumCols);
WiFiUDP udp;
String header; //HTTP request
unsigned int localPort=8888; //local port to listen for UDP packets
Ticker ticker1;
PageElement  elm;
PageBuilder  page;
String currentUri;
char ts[21],ts2[7];

void cbtick1(){
  tick1Occured=true;
}

//----------------------------------------------------------------------------------
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while(udp.parsePacket() > 0); //discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName,ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait=millis();
  while (millis()-beginWait<1500) {
    int size=udp.parsePacket();
    if(size>=NTP_PACKET_SIZE){
      Serial.println("Received NTP response");
      udp.read(packetBuffer,NTP_PACKET_SIZE); //read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900=(unsigned long)packetBuffer[40]<<24;
      secsSince1900|=(unsigned long)packetBuffer[41]<<16;
      secsSince1900|=(unsigned long)packetBuffer[42]<<8;
      secsSince1900|=(unsigned long)packetBuffer[43];
      return secsSince1900-2208988800UL+timeZone*SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP response");
  return 0; //return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer,0,NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0]=0b11100011; //LI, Version, Mode
  packetBuffer[1]=0; //Stratum, or type of clock
  packetBuffer[2]=6; //Polling Interval
  packetBuffer[3]=0xEC; //Peer Clock Precision
  //8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]=49;
  packetBuffer[13]=0x4E;
  packetBuffer[14]=49;
  packetBuffer[15]=52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address,123); //NTP requests are to port 123
  udp.write(packetBuffer,NTP_PACKET_SIZE);
  udp.endPacket();
}
//----------------------------------------------------------------------------------
int config(const char* cfgfile){
  StaticJsonDocument<1000> doc; //on stack  arduinojson.org/assistant
    Serial.println("config file");
    Serial.println(cfgfile);
  if (LittleFS.exists(cfgfile)) {
    //file exists, reading and loading
    Serial.println("Reading config file");
    File configFile=LittleFS.open(cfgfile,"r");
    if (configFile){
      resulti=0;
      resultn=0;
      Serial.println("Opened config file");
      size_t size=configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(),size);
      DeserializationError error = deserializeJson(doc, buf.get());
      if (error) {
          Serial.println("Failed to load JSON config");
          lcd.clear();
          lcd.print("Error: cfg.json");
          while(1);
      }
      JsonObject json = doc.as<JsonObject>();
      Serial.println("\nParsed json");
      strncpy(hostname,json["hostname"],11);
      hostname[10]='\0';
      Serial.println(hostname);
      if (json.containsKey("vcc"))vcc=json["vcc"];
      if (json.containsKey("vref"))vref=json["vref"];
      if (json.containsKey("r13"))r13=json["r13"];
      if (json.containsKey("r14"))r14=json["r14"];
      if (json.containsKey("dissfact"))dissfact=json["dissfact"];
      toffset=json["toffset"];
      rs=json["rs"];
      sha=json["sha"];
      shb=json["shb"];
      shc=json["shc"];
      avg=json["avg"];
      strncpy(unit,json["unit"],sizeof(unit));
      strncpy(name,json["name"],sizeof(name));
      unit[sizeof(unit)-1]='\0';
      lcdfsd=json["lcdfsd"];
      Serial.print("vcc: ");
      Serial.print(vcc,3);
      Serial.print(",vref: ");
      Serial.print(vref,3);
      Serial.print(",r13: ");
      Serial.print(r13,2);
      Serial.print(",r14: ");
      Serial.print(r14,2);
      Serial.print(",dissfact: ");
      Serial.println(dissfact,5);
      Serial.print("toffset: ");
      Serial.print(toffset,2);
      Serial.print(",rs: ");
      Serial.print(rs,5);
      Serial.print(",sha: ");
      Serial.print(sha,8);
      Serial.print(",shb: ");
      Serial.print(shb,8);
      Serial.print(",shc: ");
      Serial.println(shc,12);
      return 0;
    }
  }
  return 1;
}
//----------------------------------------------------------------------------------
String rootPage(PageArgument& args) {
  String buf;
  char line[300];

  sprintf(line,"<h3><a href=\"/config\">Configuration</a>: %s</h3><p>Time: %s Value: %0.1f %s\n<pre>",name,ts,temperature,unit);
  buf=line;
  i=resultn<RESULTL?0:resulti;
  for(j=-resultn+1;j<=0;j++){
    sprintf(line,"%s,%0.1f\n",result1[i],result2[i]);
    buf+=line;
    if(++i==RESULTL){i=0;}
  }
  buf+="</pre>";
  return buf;
}
//----------------------------------------------------------------------------------
String cfgPage(PageArgument& args) {
  String filename;
  String buf;
  char line[200];

  if (args.hasArg("filename")){
    if(!config(args.arg("filename").c_str())) buf+="<p>Done...";
    else buf+="<p>Config failed...";
  }
  else{
    Dir dir = LittleFS.openDir("/");
    buf="<h3>Click on desired configuration file:</h3>";
    while (dir.next()){
     filename=dir.fileName();
      if (filename.endsWith(".cfg")){
        Serial.println(filename);
        sprintf(line,"<p><a href=\"/config?filename=%s\">%s</a>\n",filename.c_str(),filename.c_str());
        buf+=line;
       }
    }
  }
  return buf;
}  
//----------------------------------------------------------------------------------
// This function creates dynamic web page by each request.
// It is called twice at one time URI request that caused by the structure
// of ESP8266WebServer class.
bool handleAcs(HTTPMethod method, String uri) {
  if (uri==currentUri){
    // Page is already prepared.
    return true;
  }
  else{
    currentUri=uri;
    page.clearElement();          // Discards the remains of PageElement.
    page.addElement(elm);         // Register PageElement for current access.

    Serial.println("Request:" + uri);

    if(uri=="/"){
      page.setUri(uri.c_str());
      elm.setMold(PSTR(
        "<html>"
        "<body>"
        "<h1><a href=/>Logging temperature meter (ltm)</a></h1>"
        "{{ROOT}}"
        "</body>"
        "</html>"));
      elm.addToken("ROOT", rootPage);
      return true;
    }
    else if(uri=="/config"){
      page.setUri(uri.c_str());
      elm.setMold(PSTR(
        "<html>"
        "<body>"
        "<h1><a href=/>Logging temperature meter (ltm)</a></h1>"
        "<h2>LTM1 configuration</h2>"
        "{{CONFIG}}"
        "</body>"
        "</html>"));
      elm.addToken("CONFIG",cfgPage);
      return true;
    }
    else{
      return false;    // Not found accessing exception URI.
    }
  }
}
//----------------------------------------------------------------------------------
void setup(){
  WiFiManager wifiManager;

  lcd.begin(16,2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("LTM v");
  lcd.print(ver);
  lcd.setCursor(0,1);
  lcd.print("Initialising...");

  Serial.begin(9600);
  while (!Serial){;} // wait for serial port to connect. Needed for Leonardo only
  Serial.print("\nSketch size: ");
  Serial.print(ESP.getSketchSize());
  Serial.print("\nFree size: ");
  Serial.print(ESP.getFreeSketchSpace());
  Serial.print("\n\n");
    
  if (LittleFS.begin()){
    Serial.println("Mounted file system");
    config("/default.cfg");
  }
  else{
    Serial.println("Failed to mount FS");
    lcd.clear();
    lcd.print("Failed to mount FS");
    while(1);
  }
  lcd.clear();
  lcd.print("Auto WiFi...");
  WiFi.hostname(hostname);
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect("ltmcfg");
  Serial.println("Connecting...");
  Serial.print(WiFi.hostname());
  Serial.print(" connecting to ");
  Serial.println(WiFi.SSID());
  lcd.clear();
  lcd.print("Host: ");
  lcd.print(WiFi.hostname());
//  lcd.print("Connecting...");
  lcd.setCursor(0,1);
//  lcd.print("IP: ");
  lcd.print(WiFi.localIP().toString().c_str());
  Serial.println(WiFi.localIP().toString().c_str());
//  lcd.print(" connecting to ");
//  lcd.print(WiFi.SSID());
  delay(2000);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Prepare dynamic web page
  page.exitCanHandle(handleAcs);    // Handles for all requests.
  page.insert(server);
  
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Hostname: ");
  Serial.println(WiFi.hostname());
  server.begin();

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  timeset=timeStatus()==timeSet;
  setSyncInterval(36000);

  ticker1.attach(1,cbtick1);
  lcd.clear();
  Serial.print("DateTime, ");
  Serial.println(unit);
}
//----------------------------------------------------------------------------------
void loop(){
  long prevmillis;
  
float vin;
float rt;
float self;

  prevmillis=-millis();
  if (tick1Occured == true){
    tick1Occured = false;
    t=now();
    sprintf(ts,"%04d-%02d-%02dT%02d:%02d:%02dZ",year(t),month(t),day(t),hour(t),minute(t),second(t));
    sprintf(ts2,"%02d%02d%02d",hour(t),minute(t),second(t));
    AdcAccumulator=0;
    for(i=avg;i--;){
      //read the value from the thermistor:
      AdcAccumulator+=analogRead(sensorPin);
      delay(100);
      }

    // calculate average vin
    vin=AdcAccumulator/1024.0*vref*(r13+r14)/r14/avg;
    //calculate rt
    rt=vin/((vcc-vin)/rs);
    //calculate self heating
    self=vin*vin/rt/dissfact; //zero self heating for now
    //calculate temperature - Steinhart-Hart model
    temperature=1/(sha+shb*log(rt)+shc*pow(log(rt),3))-273.15-self+toffset;
     // temperature=rt;
    
    //write circular buffer
    result2[resulti]=temperature;
    strcpy(result1[resulti],ts);
    if(++resulti==RESULTL){resulti=0;}
    if(resultn<RESULTL){resultn++;}
    if(Serial.available()<=0){
      Serial.print(ts);
      Serial.print(",");
      Serial.println(temperature,1);
      }
//revise this for temp
    // Print a message to the LCD.
    lbg.drawValue((temperature-lcdfsd+144)/3,48);// 3C per step
    lcd.setCursor(0,1);
    lcd.print(ts2);
    lcd.print(" ");
// change dB
    lcd.print(temperature,1);
    lcd.print(" ");
    lcd.print(unit);
    lcd.print("       ");
  }
  
  server.handleClient();
  prevmillis+=millis();
//    if(prevmillis>20){Serial.print("dur :"); Serial.println(prevmillis);}

}
