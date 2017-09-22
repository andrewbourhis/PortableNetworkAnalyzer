//----------------------------------------------------------------------------------------------------------------
//Nano Lab Portable Network Analyzer
//Date Created: 9-15-2017
//Author: Andrew Bourhis
//Contact: andrewbourhis@gmail.com
//----------------------------------------------------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "AD5933.h"

struct Sensor{
  unsigned long startFreq;
  unsigned long deltaFreq;
  int numbIncrements;
  int numbSettleCycles;
  int numbSettleCyclesMult;//x1, x2, or x4
  bool MCLK_int;
  unsigned long MCLK_value;
  unsigned int excitationRange;
  int PGA;//x1 or x5
  double Rfb;
  double Rcalib;
  bool midPointCalibration;//true if using simple mid-point calibration, false if using multi-point calibration

  double *gainFactor;
};
typedef Sensor Sensor;

Sensor sensor1;
Sensor sensor2;
//have 16 of these eventually

Sensor sensors[2];//array that holds all sensor structs

const int LEDPIN = 0;
const int SDAPIN = 4;
const int SCLPIN = 5;

//DO NOT FORGET TO ENTER IN WIFI INFO HERE
const char* ssid = "Xf-setup-593B";
const char* password = "Fancy5138Action";

MDNSResponder mdns;

ESP8266WebServer server(80);

const char CALIB_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<FORM action='/calib/' method='post'>"
  "<h1> waiting for calibration sweep to complete </h1>"
"</FORM>"
"</html>";

const char INDEX_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<body>"
"<h1> Nanolab Portable Network Analyzer </h1> <br>"
"<h2> Place calibration resistors in sensor bank </h2>"
"<FORM action='/calib/' method='post'>"
  "<table>"
    "<colgroup>"
      "<col span=13 style=background-color:lightblue>"
    "</colgroup>"
    "<tr>"
      "<th>Sensor Number</th>"
      "<th>Start Freq (Hz)</th>"
      "<th>Delta Freq (Hz)</th>"
      "<th># Freq Increments (9 bit)</th>"
      "<th># Settle Cycles</th>"
      "<th>Settle Cycles Multiplier</th>"
      "<th>MCLK Source</th>"
      "<th>MCLK</th>"
      "<th>Excitation Voltage</th>"
      "<th>PGA</th>"
      "<th>Feedback Resistance</th>"
      "<th>Calibration Resistance</th>"
      "<th>Type Of Calibration</th>"
    "</tr>"
    "<tr>"
      "<td>1</td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='startFreq1'></td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='deltaFreq1'></td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='numFreqInc1'></td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='numSettleCycles1'></td>"
      "<td>"
        "<select name='numbCyclesMult1'>"
          "<option value='1'>x1</option>"
          "<option value='2'>x2</option>"
          "<option value='4'>x4</option>"
        "</select>"
      "</td>"
      "<td>"
        "<select name='MCLK_sr1'>"
          "<option value='internal'>internal clock (16Mhz)</option>"
          "<option value='external'>external clock</option>"
        "</select>"
      "</td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='MCLK1'></td>"
      "<td>"
        "<select name='excitationVRange1'>"
          "<option value='1'>range 1 (2vpp)</option>"
          "<option value='2'>range 2 (1vpp)</option>"
          "<option value='3'>range 3 (0.4vpp)</option>"
          "<option value='4'>range 4 (0.2vpp)</option>"
        "</select>"
      "</td>"
      "<td>"
        "<select name='pga1'>"
          "<option value='1'>x1</option>"
          "<option value='5'>x5</option>"
        "</select>"
      "</td>"
      "<td>"
        "<select name='rfb1'>"
          "<option value='1k'>1k</option>"
          "<option value='200k'>200k</option>"
          "<option value='700k'>700k</option>"
          "<option value='1.4M'>1.4M</option>"
        "</select>"
      "</td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='Rcalib1'></td>"
      "<td>"
        "<select name='calibType1'>"
          "<option value='Mid'>Mid-point Calibration</option>"
          "<option value='Mult'>Multi-point Calibration</option>"
        "</select>"
    "</td>"
    "</tr>"

    "<tr>"
      "<td>2</td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='startFreq2'></td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='deltaFreq2'></td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='numFreqInc2'></td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='numSettleCycles2'></td>"
      "<td>"
        "<select name='numbCyclesMult2'>"
          "<option value='1'>x1</option>"
          "<option value='2'>x2</option>"
          "<option value='4'>x4</option>"
        "</select>"
      "</td>"
      "<td>"
        "<select name='MCLK_sr2'>"
          "<option value='internal'>internal clock (16Mhz)</option>"
          "<option value='external'>external clock</option>"
        "</select>"
      "</td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='MCLK2'></td>"
      "<td>"
        "<select name='excitationVRange2'>"
          "<option value='1'>range 1 (2vpp)</option>"
          "<option value='2'>range 2 (1vpp)</option>"
          "<option value='3'>range 3 (0.4vpp)</option>"
          "<option value='4'>range 4 (0.2vpp)</option>"
        "</select>"
      "</td>"
      "<td>"
        "<select name='pga2'>"
          "<option value='1'>x1</option>"
          "<option value='5'>x5</option>"
        "</select>"
      "</td>"
      "<td>"
        "<select name='rfb2'>"
          "<option value='1k'>1k</option>"
          "<option value='200k'>200k</option>"
          "<option value='700k'>700k</option>"
          "<option value='1.4M'>1.4M</option>"
        "</select>"
      "</td>"
      "<td><INPUT type='textarea' cols='10' rows='1' name='Rcalib2'></td>"
      "<td>"
        "<select name='calibType2'>"
          "<option value='Mid'>Mid-point Calibration</option>"
          "<option value='Mult'>Multi-point Calibration</option>"
        "</select>"
    "</td>"
    "</tr>"
  "</table>"
"<INPUT type='submit' value='Calibrate'>"
"</FORM>"
"</body>"
"</html>";

void handleRoot()
{
  if (server.hasArg("Rcalib1")) {
    handleSubmit();
  }
  else { //should run once on startup to load index page
    server.send(200, "text/html", INDEX_HTML);
    Serial.println("no Rcalib1 arg");
  }
}

void returnFail(String msg)
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}

//this runs when we already loaded the index page once. Maybe should attempt to reload data that was previously entered (if any)
void handleSubmit()
{
  if (!server.hasArg("Rcalib1")) return returnFail("BAD ARGS");
}

void handleCalib(){
  
  if(!getParameters()) return returnFail("BAD PARAMETERS");
  server.send(200, "text/html", CALIB_HTML);
  for(int i = 0; i < 2; i++){
    //initAD5933(i);
    
    if(sensors[i].midPointCalibration){
      //just measure/calculate gain factor at single frequency (mid point between startFreq and startFreq + deltaFreq*numbIncrements)
      //and use this value for all measurements in the sweep (doesn't account for nonlinearity of system)
      
      AD5933::calibrate();
    }
    else{
      //calculate gain factor at all frequencies in the sweep (solves nonlinearity of system's internal gain)

      
    }
    
//    Serial.println(i);
//    Serial.println(sensors[i].startFreq);
//    Serial.println(sensors[i].deltaFreq);
//    Serial.println(sensors[i].numbIncrements);
//    Serial.println(sensors[i].numbSettleCycles);
//    Serial.println(sensors[i].numbSettleCyclesMult);
//    Serial.println(sensors[i].MCLK_int);
//    Serial.println(sensors[i].MCLK_value);
//    Serial.println(sensors[i].excitationRange);
//    Serial.println(sensors[i].PGA);
//    Serial.println(sensors[i].Rfb);
//    Serial.println(sensors[i].Rcalib);
//    Serial.println(sensors[i].midPointCalibration);
  }
}

//setup sweep parameters for i'th sensor
bool initAD5933(int sensorIndex){
  byte reg1, reg2;
    if(!(AD5933::reset() &&
       AD5933::setInternalClock(sensors[sensorIndex].MCLK_int) &&
       AD5933::setPGAGain(sensors[sensorIndex].PGA) &&
       AD5933::setNumberIncrements(sensors[sensorIndex].numbIncrements) &&
       AD5933::setSettlingCycles(sensors[sensorIndex].numbSettleCycles,sensors[sensorIndex].numbSettleCyclesMult)))
       return false;

    if(sensors[sensorIndex].MCLK_int){
       if(!(AD5933::setStartFrequency(sensors[sensorIndex].startFreq) &&
          AD5933::setIncrementFrequency(sensors[sensorIndex].deltaFreq)))
          return false;
    }
    else{
      if(!(AD5933::setStartFrequency(sensors[sensorIndex].startFreq,sensors[sensorIndex].MCLK_value) &&
          AD5933::setIncrementFrequency(sensors[sensorIndex].deltaFreq,sensors[sensorIndex].MCLK_value)))
          return false;
    }
    
}

//should probably check if calibration values are valid here before initiating the calibration sweeps
bool getParameters(){
  for(int i = 0; i<2; i++){
    sensors[i].startFreq = atol(server.arg("startFreq"+String(i+1)).c_str());
    sensors[i].deltaFreq = atol(server.arg("deltaFreq"+String(i+1)).c_str());
    sensors[i].numbIncrements = atoi(server.arg("numFreqInc"+String(i+1)).c_str());
    sensors[i].numbSettleCycles = atoi(server.arg("numSettleCycles"+String(i+1)).c_str());
    sensors[i].numbSettleCyclesMult = atoi(server.arg("numbCyclesMult"+String(i+1)).c_str());
    sensors[i].MCLK_int = strcmp(server.arg("MCLK_sr"+String(i+1)).c_str(),"external");
    sensors[i].MCLK_value = atol(server.arg("MCLK"+String(i+1)).c_str());
    sensors[i].excitationRange = atoi(server.arg("excitationVRange"+String(i+1)).c_str());
    sensors[i].PGA = atoi(server.arg("pga"+String(i+1)).c_str());
    sensors[i].Rfb = atof(server.arg("rfb"+String(i+1)).c_str());
    sensors[i].Rcalib = atof(server.arg("Rcalib"+String(i+1)).c_str());
    sensors[i].midPointCalibration = strcmp(server.arg("calibType"+String(i+1)).c_str(),"Mid");
  }
  //need to implement parameter checks here and return false if something is in an incorrect format
  return true;
}

void returnOK()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK\r\n");
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void writeLED(bool LEDon)
{
  // Note inverted logic for Adafruit HUZZAH board
  if (LEDon)
    digitalWrite(LEDPIN, 0);
  else
    digitalWrite(LEDPIN, 1);
}

void setup(void)
{
  sensors[0] = sensor1;
  sensors[1] = sensor2;
  //add all sensor structs to sensors[]
  
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN,HIGH);//note, inverted logic for ESP8266 (this turns LED off)
  
  Wire.pins(SDAPIN,SCLPIN);
  Wire.begin();
  
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("TuftsNanoLab_Server", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/calib/", handleCalib);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.print("Connect to http://TuftsNanoLab_Server.local or http://");
  Serial.println(WiFi.localIP());
}

void loop(void)
{
  server.handleClient();
}
