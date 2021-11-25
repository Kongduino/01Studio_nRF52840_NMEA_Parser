#undef max
#undef min
#include <string>
#include <vector>

using namespace std;

template class basic_string<char>; // https://github.com/esp8266/Arduino/issues/1136
// Required or the code won't compile!
namespace std _GLIBCXX_VISIBILITY(default) {
_GLIBCXX_BEGIN_NAMESPACE_VERSION
void __throw_length_error(char const*) {}
void __throw_bad_alloc() {}
void __throw_out_of_range(char const*) {}
void __throw_logic_error(char const*) {}
void __throw_out_of_range_fmt(char const*, ...) {}
}

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "SPI.h"
//#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
//#include "ILI9341_t3.h"
// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 11
#define TFT_RST 10

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
// Adafruit_ILI9341
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

float latitude, longitude;
char buffer[256];
char gpsBuff[128];
char timeBuff[128];
uint8_t ix = 0;
vector<string> userStrings;
char UTC[7] = {0};
uint8_t SIV = 0;
double lastRefresh = 0;
uint8_t lineInc = 18;

void refreshDisplay() {
  uint16_t py = 25;
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(3);
  tft.println("  GNSS Test");
  tft.setTextColor(ILI9341_WHITE); tft.setTextSize(2);
  for (uint16_t i = py; py < 100; py++) tft.drawFastHLine(0, py, 240, ILI9341_BLACK);
  py = 25;
  tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.setCursor(0, py); sprintf(gpsBuff, "Latitude: %-.8f", latitude);
  tft.println(gpsBuff);
  tftPpy += lineInc;
  sprintf(gpsBuff, "Longitude: %-.8f", longitude);
  tft.setCursor(0, py); tft.println(gpsBuff);
  tftPpy += lineInc;
  tft.setCursor(0, py); tft.println(timeBuff);
  tftPpy += lineInc;
  sprintf(buffer, "SIV: %d\n", SIV);
  tft.setCursor(0, py); tft.print(buffer);
}

float parseDegrees(const char *term) {
  float value = (float)(atof(term) / 100.0);
  uint16_t left = (uint16_t)value;
  value = (value - left) * 1.66666666666666;
  value += left;
  return value;
}

vector<string> parseNMEA(string nmea) {
  vector<string>result;
  if (nmea.at(0) != '$') {
    Serial.println("Not an NMEA sentence!");
    return result;
  }
  size_t lastFound = 0;
  size_t found = nmea.find(",", lastFound);
  while (found < nmea.size() && found != string::npos) {
    string token = nmea.substr(lastFound, found - lastFound);
    result.push_back(token);
    lastFound = found + 1;
    found = nmea.find(",", lastFound);
  }
  string token = nmea.substr(lastFound, found - lastFound);
  result.push_back(token);
  lastFound = found + 1;
  found = nmea.find(",", lastFound);
  return result;
}

void parseGPRMC(vector<string>result) {
  if (result.at(1) != "") {
    sprintf(timeBuff, "%s:%s:%s UTC", result.at(1).substr(0, 2).c_str(), result.at(1).substr(2, 2).c_str(), result.at(1).substr(4, 2).c_str());
    Serial.println(timeBuff);
  }
  // if (result.at(2) == "V") Serial.println("Invalid fix!");
  // else Serial.println("Valid fix!");
  if (result.at(3) != "") {
    float newLatitude, newLongitude;
    int8_t signLat = 1, signLong = 1;
    if (result.at(4).c_str()[0] == 'W') signLat = -1;
    if (result.at(6).c_str()[0] == 'S') signLong = -1;
    newLatitude = signLat * parseDegrees(result.at(3).c_str());
    newLongitude = signLong * parseDegrees(result.at(5).c_str());
    if (newLatitude != latitude || newLongitude != longitude) {
      latitude = newLatitude;
      longitude = newLongitude;
      sprintf(gpsBuff, "Coordinates:\n%3.8f, %3.8f\n", latitude, longitude);
      Serial.print(gpsBuff);
      refreshDisplay();
    }
  }
}

void parseGPGGA(vector<string>result) {
  if (result.at(1) != "") {
    sprintf(timeBuff, "UTC Time: %s:%s:%s\n", result.at(1).substr(0, 2).c_str(), result.at(1).substr(2, 2).c_str(), result.at(1).substr(4, 2).c_str());
    Serial.print(timeBuff);
  }
  //  if (result.at(6) == "0") Serial.println("Invalid fix!");
  //  else Serial.println("Valid fix!");
  if (result.at(2) != "") {
    latitude = parseDegrees(result.at(2).c_str());
    longitude = parseDegrees(result.at(4).c_str());
    sprintf(timeBuff, "Coordinates: %3.8f %c, %3.8f %c\n", latitude, result.at(3).c_str()[0], longitude, result.at(5).c_str()[0]);
    Serial.print(timeBuff);
  }
}

void parseGPGLL(vector<string>result) {
  if (result.at(1) != "") {
    latitude = parseDegrees(result.at(1).c_str());
    longitude = parseDegrees(result.at(3).c_str());
    sprintf(gpsBuff, "Coordinates:\n%3.8f %c, %3.8f %c\n", latitude, result.at(2).c_str()[0], longitude, result.at(4).c_str()[0]);
    Serial.print(gpsBuff);
    refreshDisplay();
  }
}

void parseGPGSV(vector<string>result) {
  if (result.at(1) != "") {
    uint8_t newSIV = atoi(result.at(3).c_str());
    if (SIV != newSIV) {
      sprintf(buffer, "Message %s / %s. SIV: %s\n", result.at(2).c_str(), result.at(1).c_str(), result.at(3).c_str());
      Serial.print(buffer);
      SIV = newSIV;
    }
  }
}

void parseGPTXT(vector<string>result) {
  //$GPTXT, 01, 01, 02, ANTSTATUS = INIT
  if (result.at(1) != "") {
    sprintf(buffer, " . Message %s / %s. Severity: %s\n . Message text: %s\n",
            result.at(2).c_str(), result.at(1).c_str(), result.at(3).c_str(), result.at(4).c_str(), result.at(5).c_str());
    Serial.print(buffer);
  }
}

void parseGPVTG(vector<string>result) {
  Serial.println("Track Made Good and Ground Speed.");
  if (result.at(1) != "") {
    sprintf(buffer, " . True track made good %s [%s].\n", result.at(1).c_str(), result.at(2).c_str());
    Serial.print(buffer);
  }
  if (result.at(3) != "") {
    sprintf(buffer, " . Magnetic track made good %s [%s].\n", result.at(3).c_str(), result.at(4).c_str());
    Serial.print(buffer);
  }
  if (result.at(5) != "") {
    sprintf(buffer, " . Speed: %s %s.\n", result.at(5).c_str(), result.at(6).c_str());
    Serial.print(buffer);
  }
  if (result.at(7) != "") {
    sprintf(buffer, " . Speed: %s %s.\n", result.at(7).c_str(), result.at(8).c_str());
    Serial.print(buffer);
  }
}

void parseGPGSA(vector<string>result) {
  // $GPGSA,A,3,15,29,23,,,,,,,,,,12.56,11.96,3.81
  Serial.println("GPS DOP and active satellites");
  if (result.at(1) == "A") Serial.println(" . Mode: Automatic");
  else if (result.at(1) == "M") Serial.println(" . Mode: Manual");
  else Serial.println(" . Mode: ???");
  if (result.at(2) == "1") {
    Serial.println(" . Fix not available.");
    return;
  } else if (result.at(2) == "2") Serial.println(" . Fix: 2D");
  else if (result.at(2) == "3") Serial.println(" . Fix: 3D");
  else {
    Serial.println(" . Fix: ???");
    return;
  }
  Serial.print(" . PDOP: "); Serial.println(result.at(result.size() - 3).c_str());
  Serial.print(" . HDOP: "); Serial.println(result.at(result.size() - 2).c_str());
  Serial.print(" . VDOP: "); Serial.println(result.at(result.size() - 1).c_str());
}

void setup() {
  tft.begin();
  tft.setTextWrap(true);
  tft.setRotation(2);
  refreshDisplay();
  Serial.begin(115200);
  time_t timeout = millis();
  while (!Serial) {
    if ((millis() - timeout) < 5000) {
      delay(100);
    } else {
      break;
    }
  }
  delay(2500);
  Serial.println("\nGPS Example (NMEA Parser)");
  Serial1.begin(9600);
  delay(100);
  Serial.println("Serial1 ready!");
}

bool waitForDollar = true;

void loop() {
  double t0 = millis();
  if (t0 - lastRefresh > 9999) {
    refreshDisplay();
    lastRefresh = millis();
  }
  if (Serial1.available()) {
    char c = Serial1.read();
    if (waitForDollar && c == '$') {
      waitForDollar = false;
      buffer[0] = '$';
      ix = 1;
    } else if (waitForDollar == false) {
      if (c == 13) {
        buffer[ix] = 0;
        c = Serial1.read();
        delay(50);
        string nextLine = string(buffer);
        userStrings.push_back(nextLine.substr(0, nextLine.size() - 3));
        waitForDollar = true;
      } else {
        buffer[ix++] = c;
      }
    }
  }
  if (userStrings.size() > 0) {
    string nextLine = userStrings[0];
    userStrings.erase(userStrings.begin());
    if (nextLine.substr(0, 1) != "$") {
      // Serial.print("Not an NMEA string!\n>> ");
      // Serial.println(nextLine.c_str());
      return;
    } else {
      vector<string>result = parseNMEA(nextLine);
      if (result.size() == 0) return;
      string verb = result.at(0);
      if (verb.substr(3, 3) == "RMC") {
        parseGPRMC(result);
      } else if (verb.substr(3, 3) == "GSV") {
        parseGPGSV(result);
      } else if (verb.substr(3, 3) == "GGA") {
        parseGPGGA(result);
      } else if (verb.substr(3, 3) == "GLL") {
        parseGPGLL(result);
      } else if (verb.substr(3, 3) == "GSA") {
        parseGPGSA(result);
      } else if (verb.substr(3, 3) == "VTG") {
        parseGPVTG(result);
      } else if (verb.substr(3, 3) == "TXT") {
        parseGPTXT(result);
      } else {
        Serial.println(nextLine.c_str());
      }
    }
  }
}
