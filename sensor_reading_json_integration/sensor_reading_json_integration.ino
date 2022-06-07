//esp server and sensor reading JSON integration

//setup parameters
//#define debug
#define ap
//#define static_ap

//include libraries
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SparkFunBME280.h"
#include "ccs811.h"
#include "Adafruit_PM25AQI.h"
#include <SoftwareSerial.h>
#include "dashboard.h"

//define the devices
BME280 pres;
CCS811 ccs811(0);
SoftwareSerial pmSerial(2, 3);
Adafruit_PM25AQI pm_sens = Adafruit_PM25AQI();
ESP8266WebServer server(80);

//create the JSON document with the default info
StaticJsonDocument<500> doc;
StaticJsonDocument<500> wifi_data;
String input = "{\"temperature\":0,\"humidity\":0,\"pm1\":0,\"pm2_5\":0,\"pm10\":0,\"pressure\":0,\"co2\":0,\"voc\":0,\"dew_point\":0}";
DeserializationError error = deserializeJson(doc, input);

//wifi creds for ap and static modes
const char* ssid = "Bearsden 2.0"; //Enter Wi-Fi SSID
const char* password =  "Peace10310313"; //Enter Wi-Fi Password

//const ap ssid pass
const char *ap_ssid = "AirMonitor";
const char *ap_password = "12345678";

//create a struct for the data
struct sensor_readings {
  int pm1 = 0;
  int pm25 = 0;
  int pm10 = 0;
  uint16_t eco2 = 0;
  uint16_t tvoc = 0;
  float humidity = 0.0;
  float pressure = 0.0;
  float temp = 0.0;
  float dew_point = 0.0;
};

//create the struct object
sensor_readings readings;


//sensor reading function
void sensor_read() {
  //read the pm 2.5 sensor
  PM25_AQI_Data data;

  if (! pm_sens.read(&data)) {
#ifdef debug
    Serial.println("Could not read from AQI");
#endif
    delay(500);  // try again in a bit!
    return;
  }

#ifdef debug
  Serial.println("AQI reading success");
  Serial.println();
  Serial.println(F("---------------------------------------"));
  Serial.println(F("Concentration Units (standard)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_standard);
  Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_standard);
  Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_standard);
  Serial.println(F("Concentration Units (environmental)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_env);
  Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_env);
  Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_env);
  Serial.println(F("---------------------------------------"));
  Serial.print(F("Particles > 0.3um / 0.1L air:")); Serial.println(data.particles_03um);
  Serial.print(F("Particles > 0.5um / 0.1L air:")); Serial.println(data.particles_05um);
  Serial.print(F("Particles > 1.0um / 0.1L air:")); Serial.println(data.particles_10um);
  Serial.print(F("Particles > 2.5um / 0.1L air:")); Serial.println(data.particles_25um);
  Serial.print(F("Particles > 5.0um / 0.1L air:")); Serial.println(data.particles_50um);
  Serial.print(F("Particles > 10 um / 0.1L air:")); Serial.println(data.particles_100um);
  Serial.println(F("---------------------------------------"));
#endif
  readings.pm1 = data.pm10_standard;
  readings.pm25 = data.pm25_standard;
  readings.pm10 = data.pm100_standard;

  //read the ccs811
  uint16_t eco2, etvoc, errstat, raw;
  ccs811.read(&eco2, &etvoc, &errstat, &raw);
  if ( errstat == CCS811_ERRSTAT_OK ) {
#ifdef debug
    Serial.print("CCS811: ");
    Serial.print("eco2=");  Serial.print(eco2);     Serial.print(" ppm  ");
    Serial.print("etvoc="); Serial.print(etvoc);    Serial.print(" ppb  ");
    Serial.println();
#endif
    readings.eco2 = eco2;
    readings.tvoc = etvoc;
  }
#ifdef debug
  else if ( errstat == CCS811_ERRSTAT_OK_NODATA ) {
    Serial.println("CCS811: waiting for (new) data");
  } else if ( errstat & CCS811_ERRSTAT_I2CFAIL ) {
    Serial.println("CCS811: I2C error");
  } else {
    Serial.print("CCS811: errstat="); Serial.print(errstat, HEX);
    Serial.print("="); Serial.println( ccs811.errstat_str(errstat) );
  }
#endif
  //read from bmp280
  float hum = pres.readFloatHumidity();
  readings.humidity = hum;

  float pressure = pres.readFloatPressure();
  readings.pressure = pressure;

  float temp = pres.readTempC();
  readings.temp = temp;

  float dewpoint = pres.dewPointC();
  readings.dew_point = dewpoint;
#ifdef debug
  Serial.print("Humidity: ");
  Serial.print(hum, 0);

  Serial.print(" Pressure: ");
  Serial.print(pressure, 0);

  Serial.print(" Temp: ");
  Serial.print(temp, 2);

  Serial.print(" Dew point: ");
  Serial.print(dewpoint, 2);
  Serial.println();
#endif
}

/*
   int pm1 = 0;
  int pm25 = 0;
  int pm10 = 0;
  uint16_t eco2 = 0;
  uint16_t tvoc = 0;
  float humidity = 0.0;
  float pressure = 0.0;
  float temp = 0.0;
  float dew_point = 0.0;
*/
//json updater function
void update_json_doc() {
  doc["temperature"] = readings.temp;
  doc["humidity"] = readings.humidity;
  doc["pm1"] = readings.pm1;
  doc["pm2_5"] = readings.pm25;
  doc["pm10"] = readings.pm10;
  doc["pressure"] = readings.pressure;
  doc["co2"] = readings.eco2;
  doc["voc"] = readings.tvoc;
  doc["dew_point"] = readings.dew_point;
};

//web server path handler functions
void handledata() {
  String json;
  //doc.prettyPrintTo(json);
  String output;
  serializeJson(doc, output);
  Serial.println(output);
  server.send(200, "text/json", output);
  //server.send(200, "text/plain", "Hello world");
  update_json_doc();
}

void handleroot() {
  server.send(200, "text/html", dashboard);
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
#ifdef debug
  Serial.begin(115200);
#endif
#ifdef static_ap
  //in connect mode
  //with static ssid, pass
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

#endif

#ifdef ap
  //ap mode
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("IP address: ");
  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP);
#endif

  server.on("/data", handledata);      //Which routine to handle at root location which is the dashbooard
  server.on("/", handleroot);

  server.begin();                  //Start server
#ifdef debug
  Serial.println("HTTP server started");
#endif
  //init sensors
  //initialize and config I2c
  Wire.begin();
  //since esp8266 does not handle clock streatching automatically stretch the clock manually
  //initializing each sensor
  ccs811.set_i2cdelay(50);
  pres.setI2CAddress(0x76);
  //setting up the pm sensor to read at baud of 9600
  pmSerial.begin(9600);


  //check if each sensor began correctly and yell if something goes wrong
  if (pres.beginI2C() == false) Serial.println("BME280 failed to initialize");
  if (!pm_sens.begin_UART(&pmSerial)) Serial.println("PM 2.5/10 sensor failed to initialize");
  if (!ccs811.begin()) Serial.println("CCS811 failed to initialize");
  //check for startup of the ccs811 sensor in 1 second mode
  if (!ccs811.start(CCS811_MODE_1SEC)) Serial.println("CCS811 failed to initialize.");
}

int last_update = 0;
void loop() {
  if ((millis() - last_update) > 300) {
    sensor_read();
    update_json_doc();

  };
  // put your main code here, to run repeatedly:
  server.handleClient();
}
