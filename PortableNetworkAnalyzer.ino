//----------------------------------------------------------------------------------------------------------------
//Nano Lab Portable Network Analyzer
//Date Created: 9-15-2017
//Author: Andrew Bourhis
//Contact: andrewbourhis@gmail.com
//Git repo: https://github.com/andrewbourhis/PortableNetworkAnalyzer
//----------------------------------------------------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "AD5933.h"//ensure these files (need the .cpp file as well) are in the same folder as this program

//----------------------------------------------------------------------------------------------------------------
#define MAX14661_ADDRESS (0x4C)
//multiplexer I2C addresses
#define MAX14661_READ_ADDR (0x99)
#define MAX14661_WRITE_ADDR (0x98)
//multiplexer I2C command register addresses
#define MAX14661_CMD_A  (0x14)
#define MAX14661_CMD_B (0x15)
//multiplexer I2C commands (see Table 2 in MAX14661 datasheet)
#define MAX14661_CLEAR_B  (0x10)
//----------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------
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
};
typedef Sensor Sensor;
//----------------------------------------------------------------------------------------------------------------


Sensor sensors[16];//array that holds all sensor structs (allocate enough room for 16 of them)

int numberOfSensors = 1;//this is the number of sweeps to perform (user defines dynamically)
bool startedCalibrating = false, doneCalibrating = false, startedSweeping = false, doneSweeping = false;
//output LED used for debugging/signaling to user (also used for bootloading)
const int LEDPIN = 0;
//I2C pins
const int SDAPIN = 4;
const int SCLPIN = 5;
//address pins for MAX4734 (U3)
const int A0PIN = 16;
const int A1PIN = 14;

//DO NOT FORGET TO ENTER IN WIFI INFO HERE
const char* ssid = "Xf-setup-593B";
const char* password = "Fancy5138Action";

//used to create wifi server
MDNSResponder mdns;
ESP8266WebServer server(80);

//these are dynamically modified html files based on user input (i.e. parameters, number of sensors, etc..)
//note that since they are dynamically changed, the characters MUST be stored in RAM, which can get filled
//up quickly. Therefore, it is necessary to minimize the amount of RAM that is used to prevent seg faults
String SWEEP_DONE_HTML;
String CALIB_DONE_HTML;
String GETPARAMS_HTML;

//----------------------------------------------------------------------------------------------------------------
//The following constant character arrays are stored in program memory and are non-modifiable.
const char SWEEP_HTML[] PROGMEM =
"<!DOCTYPE html>"
"<html>"
"<FORM action='/sweep-start/' method='post'>"
  "<h1>Ready to perform sweep</h1><br>"
  "<h2>Please replace calibration resistors with sensors before continuing</h2>"
  "<INPUT type='submit' id='run' value='Run Sweep'>"
"</FORM>"
"</html>";

const char SWEEP_START_HTML[] PROGMEM =
"<!DOCTYPE html>"
"<html>"
"<meta http-equiv='refresh' content='5'>"
"<h1>Performing frequency sweeps - please wait</h1>"

"<style>"
  ".loader {border: 16px solid #f3f3f3;border-radius: 50%;border-top: 16px solid #3498db;width: 120px;"
  "height: 120px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite;}"
  
  "@-webkit-keyframes spin {0% { -webkit-transform: rotate(0deg); }"
    "100% { -webkit-transform: rotate(360deg); }}"
  
  "@keyframes spin {0% { transform: rotate(0deg); }"
    "100% { transform: rotate(360deg); }}"
"</style>"
"<div class='loader'></div>"
"</html>";

const char CALIB_HTML[] PROGMEM =
"<!DOCTYPE html>"
"<html>"
"<meta http-equiv='refresh' content='5'>"
"<h1>Performing calibration sweeps - please wait</h1>"

"<style>"
  ".loader {border: 16px solid #f3f3f3;border-radius: 50%;border-top: 16px solid #3498db;width: 120px;"
  "height: 120px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite;}"
  
  "@-webkit-keyframes spin {0% { -webkit-transform: rotate(0deg); }"
    "100% { -webkit-transform: rotate(360deg); }}"
  
  "@keyframes spin {0% { transform: rotate(0deg); }"
    "100% { transform: rotate(360deg); }}"
"</style>"
"<div class='loader'></div>"
"</html>";
  
const char INDEX_HTML[] PROGMEM =
"<!DOCTYPE html>"
"<html>"
"<body>"
"<h1> Nanolab Portable Network Analyzer </h1> <br>"
"<h2> How many sensors do you wish to characterize? </h2>"
"<FORM action='/get-params/' method='post'>"
  "<select name='numSensors'>"
    "<option value='1'>1</option>"
    "<option value='2'>2</option>"
    "<option value='3'>3</option>"
    "<option value='4'>4</option>"
    "<option value='5'>5</option>"
    "<option value='6'>6</option>"
    "<option value='7'>7</option>"
    "<option value='8'>8</option>"
    "<option value='9'>9</option>"
    "<option value='10'>10</option>"
    "<option value='11'>11</option>"
    "<option value='12'>12</option>"
    "<option value='13'>13</option>"
    "<option value='14'>14</option>"
    "<option value='15'>15</option>"
    "<option value='16'>16</option>"
  "</select>"
  "<INPUT type='submit' value='Continue'>"
"</FORM>"
"</body>"
"</html>";

const char paramConstSTR[] PROGMEM = "<!DOCTYPE html>"
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
    "</tr>";

//----------------------------------------------------------------------------------------------------------------

/**
 * Handle client connection by sending the index page to be loaded on client browser. This is where the user
 * enters the number of sensors to sweep.
 *
 * @return NULL
 */
void handleRoot()
{
  server.send(200, "text/html", INDEX_HTML);  
}

/**
 * Handle the process of acquiring parameters by setting up the html input table. Note that GETPARAMS_HTML 
 * is a dynamic size, and can begin to take up a LOT of RAM, so it should be cleared after it is uploaded
 * to the webserver. Doing so will clear up space for the sweep data that must be stored in RAM later on.
 *
 * @return NULL
 */
void handleGetParams(){
  numberOfSensors = atoi(server.arg("numSensors").c_str());
  if (!server.hasArg("Rcalib1")){
    
    GETPARAMS_HTML = FPSTR(paramConstSTR);
    for(int i = 1; i <= numberOfSensors; i++){
      GETPARAMS_HTML += F("<tr><td>");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("</td><td><INPUT type='textarea' cols='10' rows='1' name='startFreq");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'></td><td><INPUT type='textarea' cols='10' rows='1' name='deltaFreq");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'></td><td><INPUT type='textarea' cols='10' rows='1' name='numFreqInc");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'></td><td><INPUT type='textarea' cols='10' rows='1' name='numSettleCycles");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'></td><td><select name='numbCyclesMult");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'><option value='1'>x1</option><option value='2'>x2</option><option value='4'>x4</option></select></td><td><select name='MCLK_sr");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'><option value='internal'>internal clock (16Mhz)</option><option value='external'>external clock</option></select></td><td><INPUT type='textarea' cols='10' rows='1' name='MCLK");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'></td><td><select name='excitationVRange");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'><option value='1'>range 1 (2vpp)</option><option value='2'>range 2 (1vpp)</option><option value='3'>range 3 (0.4vpp)</option><option value='4'>range 4 (0.2vpp)</option></select></td><td><select name='pga");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'><option value='1'>x1</option><option value='5'>x5</option></select></td><td><select name='rfb");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'><option value='1000'>1k</option><option value='200000'>200k</option><option value='700000'>700k</option><option value='1400000'>1.4M</option></select></td><td><INPUT type='textarea' cols='10' rows='1' name='Rcalib");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'></td><td><select name='calibType");
      GETPARAMS_HTML += String(i);
      GETPARAMS_HTML += F("'><option value='Mid'>Mid-point Calibration</option><option value='Mult'>Multi-point Calibration</option></select></td></tr>");
    }
    GETPARAMS_HTML += F("</table><INPUT type='submit' value='Calibrate'></FORM></body></html>");
  }
  server.send(200, "text/html", GETPARAMS_HTML);
  GETPARAMS_HTML = "";//clear html buffer to provide mem space for sweeps
  //may want to use memset(GETPARAMS_HTML,0,sizeof(GETPARAMS_HTML));
}

/**
 * Sends a failure message to the webserver
 *
 * @param msg this is the error message to send to the webserver
 * @return NULL
 */
void returnFail(String msg)
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, F("text/plain"), msg + "\r\n");
}


/**
 * Populates the CALIB_DONE_HTML string by first running a calibration sweep via makeCalibBuff(), then placing the results into a textarea
 * and adding a button to download the calibration sweep data.
 *
 * @return NULL
 */
void sendCalibData(){
  CALIB_DONE_HTML += makeCalibBuff();
  CALIB_DONE_HTML += F("</textarea><INPUT type='submit' id='save' value='Download'><script type='text/javascript'>");  
  CALIB_DONE_HTML += F("var str = document.getElementById('source').value;var uri = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);");
  CALIB_DONE_HTML += F("var downloadLink = document.createElement('a');document.getElementById('save').onclick = function(){");
  CALIB_DONE_HTML += F("downloadLink.href = uri;downloadLink.download = 'calibData.csv';document.body.appendChild(downloadLink);");
  CALIB_DONE_HTML += F("downloadLink.click();document.body.removeChild(downloadLink);}</script></FORM></html>");
  
}

/**
 * This is the function that runs the calibration sweep. Additionally, it formats the data to be downloaded in the following format:
 * formatting: sensor i's start freq, sensor i's end freq, sensor i's calib resistance, sensor i's rfb, sensor i's number of measurements in sweep,
 * sensor i's array of sweep measurements (real, imag, real, imag, ...)
 * 
 * Note that a ',' delimiter is used between elements, and a '/n' is used between sensors so that the matlab script can parse it
 *
 * @return tmp This is a calibration data buffer which contains all of the above data in the specified format
 */
String makeCalibBuff(){
  String tmp;
  int numCalibPoints = 1;
  for(int i = 0; i < numberOfSensors; i++){
    unsigned long endFreq = sensors[i].startFreq + (sensors[i].numbIncrements * sensors[i].deltaFreq);
    if(sensors[i].midPointCalibration)
      numCalibPoints = 1;
    else
      numCalibPoints = sensors[i].numbIncrements + 1;

    tmp += String(sensors[i].startFreq) + ",";
    tmp += String(endFreq) + ",";
    tmp += String(sensors[i].Rcalib) + ",";
    tmp += String(sensors[i].Rfb) + ",";
    tmp += String(sensors[i].numbIncrements + 1) + ",";

    if(!initAD5933(i))
      Serial.println(F("could not initialize AD5933"));
    setRfbMux(sensors[i].Rfb);
    delay(1);
    int real, imag, j = 1;
    // Initialize the frequency sweep
    if (!(AD5933::setPowerMode(POWER_STANDBY) &&          // place in standby
          AD5933::setControlMode(CTRL_INIT_START_FREQ) && // init start freq
          AD5933::setControlMode(CTRL_START_FREQ_SWEEP))) // begin frequency sweep
         {
             Serial.println(F("Could not initialize frequency sweep..."));
         }

    // Perform the actual sweep
    while ((AD5933::readStatusRegister() & STATUS_SWEEP_DONE) != STATUS_SWEEP_DONE) {
        // Get the frequency data for this frequency point (will spin here until valid data available)
        if (!AD5933::getComplexData(&real, &imag)) {
            Serial.println(F("Could not get raw frequency data..."));
        }
        //for some reason, when we set up mid-point calibration, it needs to take two measurements rather than a single measurement
        //still need to figure out how to properly set up the AD5933 to only take a single measurement
        if(j > numCalibPoints)
          break;
          
        tmp += String(real);
        tmp += F(",");
        tmp += String(imag);
        
        if(j == numCalibPoints)
          tmp += F("&#13;");
        else
          tmp += F(",");
        
        // Increment the frequency
        AD5933::setControlMode(CTRL_INCREMENT_FREQ);
        j++;
    }

  // Set AD5933 power mode to standby when finished
  if (!AD5933::setPowerMode(POWER_STANDBY))
      Serial.println(F("Could not set to standby..."));
  }
  return tmp;
}

/**
 * Handles the sweep page load
 *
 * @return NULL
 */
void handleSweep(){
  server.send(200,"text/html",SWEEP_HTML);
}

/**
 * Runs the sweeps by calling makeSweepBuff() and will take some time. Similar to the sendCalibData() function
 *
 * @return NULL
 */
void sendSweepData(){
  SWEEP_DONE_HTML += makeSweepBuff();
  SWEEP_DONE_HTML += F("</textarea><INPUT type='submit' id='save' value='Download'><script type='text/javascript'>");
  SWEEP_DONE_HTML += F("var str = document.getElementById('source').value;var uri = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);");
  SWEEP_DONE_HTML += F("var downloadLink = document.createElement('a');document.getElementById('save').onclick = function(){");
  SWEEP_DONE_HTML += F("downloadLink.href = uri;downloadLink.download = 'sweepData.csv';document.body.appendChild(downloadLink);");
  SWEEP_DONE_HTML += F("downloadLink.click();document.body.removeChild(downloadLink);}</script></html>");
}

/**
 * Handles the sweeping by first sending SWEEP_START_HTML to the webserver, then running the sweep through sendSweepData()
 * Once the sweep is complete, the doneSweeping boolean is set to true, and on the next reload, the webserver will upload
 * SWEEP_DONE_HTML with the sweep data, and the user can download this data into a csv file.
 *
 * @return NULL
 */
void handleStartSweep(){
  if(doneSweeping){
    server.send(200,"text/html",SWEEP_DONE_HTML);
  }
  else if(!startedSweeping){
    server.send(200,"text/html",SWEEP_START_HTML);
    startedSweeping = true;
    sendSweepData();
    doneSweeping = true;
  }
  else
    server.send(200,"text/html",SWEEP_START_HTML);
}


/**
 * Generates the sweep buffer, which is formatted as a csv as follows:
 * real value is first, then imaginary value comes second (after a comma) for each freq bin
 * and the 0th sensor is the first line, 1st sensor is the second line, etc...
 *
 * @return NULL
 */
String makeSweepBuff(){
  
  String retStr;
  for(int i = 0; i < numberOfSensors; i++){
    if(!initAD5933(i))
      Serial.println(F("could not initialize AD5933"));
    setRfbMux(sensors[i].Rfb);
    delay(1);
    int real, imag, j = 1;
    // Initialize the frequency sweep
    if (!(AD5933::setPowerMode(POWER_STANDBY) &&          // place in standby
          AD5933::setControlMode(CTRL_INIT_START_FREQ) && // init start freq
          AD5933::setControlMode(CTRL_START_FREQ_SWEEP))) // begin frequency sweep
         {
             Serial.println(F("Could not initialize frequency sweep..."));
         }

    // Perform the actual sweep
    while ((AD5933::readStatusRegister() & STATUS_SWEEP_DONE) != STATUS_SWEEP_DONE) {
        // Get the frequency data for this frequency point
        if (!AD5933::getComplexData(&real, &imag)) {
            Serial.println(F("Could not get raw frequency data..."));
        }

        retStr += String(real);
        retStr += F(",");
        retStr += String(imag);
        
        if(j == sensors[i].numbIncrements + 1)
          retStr += F("&#13;");
        else
          retStr += F(",");
        
        // Increment the frequency
        AD5933::setControlMode(CTRL_INCREMENT_FREQ);
        j++;
    }

    Serial.println(F("Frequency sweep complete!"));

  }
  // Set AD5933 power mode to standby when finished
  if (!AD5933::setPowerMode(POWER_STANDBY))
      Serial.println(F("Could not set to standby..."));
  return retStr;
}


/**
 * Handles the calibration process by first sending CALIB_HTML, then gathering the user parameters, then running the
 * calibration sweep via sendCalibData(). This data is loaded into the CALIB_DONE_HTML buffer, and is sent to the
 * server upon the next reload (once the calibration sweep is complete, and the doneCalibrating boolean is set true).
 *
 * @return NULL
 */
void handleCalib(){
    Serial.println(F("calibrate"));
  if(doneCalibrating){
    server.send(200,"text/html",CALIB_DONE_HTML);
    CALIB_DONE_HTML = "";//free up space for sweep.
  }
  else if(!startedCalibrating){
    Serial.println(F("starting to calibrate"));
    server.send(200, "text/html", CALIB_HTML);
    startedCalibrating = true;
    if(!getParameters()) return returnFail(F("BAD PARAMETERS"));
    Serial.println(F("got parameters"));
    sendCalibData();//takes some time to complete all calibration sweeps
    doneCalibrating = true;
    Serial.println(F("done calibrating"));
  }
  else
    server.send(200, "text/html", CALIB_HTML);
}


/**
 * Sets the feedback resistance by writing the proper address to the Rfb multiplexer.
 *
 * @return bool true if successful, false if not (i.e. if user enters incorrect rfb value)
 */
bool setRfbMux(double rfb){
  switch(int(rfb)){
    case 1000:
      digitalWrite(A0PIN,LOW);
      digitalWrite(A1PIN,LOW);
      return true;
    case 200000:
      digitalWrite(A0PIN,HIGH);
      digitalWrite(A1PIN,LOW);
      return true;
    case 700000:
      digitalWrite(A0PIN,LOW);
      digitalWrite(A1PIN,HIGH);
      return true;
    case 1400000:
      digitalWrite(A0PIN,HIGH);
      digitalWrite(A1PIN,HIGH);
      return true;
    default:
      digitalWrite(A0PIN,LOW);
      digitalWrite(A1PIN,HIGH);
      return false;
  }
}

/**
 * Initializes the AD5933 for the specified sensor (assumes that the sweep parameters are already uploaded and stored in sensors[]
 * Note that if the user wishes to perform a mid-point calibration rather than calibrating for every sweep measurement, then
 * this function will check if the calibration sweep has completed yet, and if it has not, it will set the number of frequency
 * increments to 0, so that the AD5933 will only take a single measurement in the midpoint between startFreq and endFreq.
 *
 * @param sensorIndex this is the index of the sensor that we are setting up the AD5933 to sweep.
 * @return bool true if the initialization is successful. False if otherwise.
 */
bool initAD5933(int sensorIndex){
  int nInc = sensors[sensorIndex].numbIncrements;
  long startFreqVal = sensors[sensorIndex].startFreq;
  long delFreq = sensors[sensorIndex].deltaFreq;
  long midFreqVal = startFreqVal + ( ( sensors[sensorIndex].numbIncrements * sensors[sensorIndex].deltaFreq ) / 2 );
  muxSelect(sensorIndex);
  //if we are calibrating, make sure we only take a single sample for mid-point calibration
  if(sensors[sensorIndex].midPointCalibration && !doneCalibrating){
    Serial.println(F("calibrating init"));
    startFreqVal = midFreqVal;
    nInc = 0;
  }
  
  if(!(AD5933::reset() &&
     AD5933::setInternalClock(sensors[sensorIndex].MCLK_int) &&
     AD5933::setPGAGain(sensors[sensorIndex].PGA) &&
     AD5933::setNumberIncrements(nInc) &&
     AD5933::setSettlingCycles(sensors[sensorIndex].numbSettleCycles,sensors[sensorIndex].numbSettleCyclesMult)))
     return false;
  
  if(!sensors[sensorIndex].MCLK_int){
    Serial.println(F("MCLK_int is false"));
    if(!(AD5933::setStartFrequency(startFreqVal,sensors[sensorIndex].MCLK_value) &&
        AD5933::setIncrementFrequency(delFreq,sensors[sensorIndex].MCLK_value)))
        return false;
  }
  else{
    Serial.println(F("MCLK_int is true"));
    if(!(AD5933::setStartFrequency(startFreqVal) &&
      AD5933::setIncrementFrequency(delFreq)))
      return false;
  }
    
}

/**
 * Gathers the data from the webserver and loads it into the sensor[] structure array
 * To do: Implement a check to determine if the calibration values are valid prior to initiating the
 * calibration sweeps.
 *
 * @return bool true if successfully acquired all parameters, false if otherwise
 */
bool getParameters(){
  for(int i = 0; i < numberOfSensors; i++){
    sensors[i].startFreq = atol(server.arg("startFreq"+String(i+1)).c_str());
    sensors[i].deltaFreq = atol(server.arg("deltaFreq"+String(i+1)).c_str());
    sensors[i].numbIncrements = atoi(server.arg("numFreqInc"+String(i+1)).c_str());
    sensors[i].numbSettleCycles = atoi(server.arg("numSettleCycles"+String(i+1)).c_str());
    sensors[i].numbSettleCyclesMult = atoi(server.arg("numbCyclesMult"+String(i+1)).c_str());
    sensors[i].MCLK_int = strcmp(server.arg("MCLK_sr"+String(i+1)).c_str(),"external");
    
    if(server.arg("MCLK"+String(i+1)).c_str() == "")
      sensors[i].MCLK_value = 16776000;
    else
      sensors[i].MCLK_value = atol(server.arg("MCLK"+String(i+1)).c_str());
    
    sensors[i].excitationRange = atoi(server.arg("excitationVRange"+String(i+1)).c_str());
    sensors[i].PGA = atoi(server.arg("pga"+String(i+1)).c_str());
    sensors[i].Rfb = atof(server.arg("rfb"+String(i+1)).c_str());
    sensors[i].Rcalib = atof(server.arg("Rcalib"+String(i+1)).c_str());
    sensors[i].midPointCalibration = strcmp(server.arg("calibType"+String(i+1)).c_str(),"Mult");
  }
  //need to implement parameter checks here and return false if something is in an incorrect format
  return true;
}


/**
 * Handles situation when a handle is not found (i.e. on a FORM action, or incorrect URL)
 *
 * @return NULL
 */
void handleNotFound()
{
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + F("\n");
  }
  server.send(404, F("text/plain"), message);
}


/**
 * Writes value to LED (not really used, but may be useful for debugging)
 *
 * @param bool true to turn on, false to turn off
 * @return NULL
 */
void writeLED(bool LEDon)
{
  // Note inverted logic for Adafruit HUZZAH board
  if (LEDon)
    digitalWrite(LEDPIN, 0);
  else
    digitalWrite(LEDPIN, 1);
}

/**
 * initializes the MAX14661 to enable SW01A only (disable bank B)
 * 
 * This is just a wrapper for muxSelect
 *
 * @return NULL
 */
void initSensorMux(){
  muxSelect(0);
}

/**
 * Writes to the MAX14661 over I2C to select the i'th sensor index
 *
 * @param sensorIndex this is the index of the sensor that the MUX will select
 * @return NULL
 */
void muxSelect(int sensorIndex){
  Wire.beginTransmission(MAX14661_ADDRESS);
  byte cmdA = (sensorIndex & 0x0F);
  //Wire.write(MAX14661_WRITE_ADDR);
  Wire.write(MAX14661_CMD_A);
  Wire.write(cmdA);
  Wire.write(MAX14661_CLEAR_B);
  Wire.endTransmission();
}

/**
 * Sets up the DDRs, dynamic strings, serial port, webserver, and MUX.
 *
 * @return NULL
 */
void setup(void)
{  
  SWEEP_DONE_HTML = F("<!DOCTYPE html> <html><h1> Done sweeping! Please download sweep data</h1><textarea cols='50' rows='10' id='source' name = 'sweepData' readonly>");
  CALIB_DONE_HTML = F("<!DOCTYPE html> <html> <FORM action='/sweep/' method='post'><h2> Done Calibrating! Please download calibration data</h1><textarea cols='50' rows='10' id='source' name = 'gainFactors' readonly>");
  
  pinMode(LEDPIN, OUTPUT);
  pinMode(A0PIN, OUTPUT);
  pinMode(A1PIN, OUTPUT);
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
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  if (mdns.begin("TuftsNanoLab_Server", WiFi.localIP())) {
    Serial.println(F("MDNS responder started"));
  }

  server.on("/", handleRoot);
  server.on("/calib/", handleCalib);
  server.on("/sweep/", handleSweep);
  server.on("/sweep-start/", handleStartSweep);
  server.on("/get-params/", handleGetParams);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.print(F("Connect to http://TuftsNanoLab_Server.local or http://"));
  Serial.println(WiFi.localIP());

  //initialize sensor bank
  initSensorMux();
}

void loop(void)
{
  server.handleClient();
}
