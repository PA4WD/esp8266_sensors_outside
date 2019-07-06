#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include "Adafruit_SHT31.h"
#include <InfluxDb.h>
#include "credentials.h"

#define DEBUG

Ticker pushTimer;
#define INTERVALTIME 300 //300sec between updates

//BMP085 SCl = D5
//BMP085 SDA = D6
Adafruit_BMP085 bmp;
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
Adafruit_SHT31 sht31 = Adafruit_SHT31();

int timerFlag = 0;
const char ssid[] = WIFI_SSID;
const char password[] = WIFI_PASSWD;
String chipid;
Influxdb influx(INFLUXDB_HOST);

// connect to wifi network
void connectWifi()
{
  // attempt to connect to Wifi network:
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Connect to WEP/WPA/WPA2 network:
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

float GetLuxValue()
{
  Wire.begin(D4, D7);

  /* Initialise the sensor */
  if (!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while (1)
      ;
  }

  /* You can also manually set the gain or enable auto-gain support */
  //tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  //tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true); /* Auto-gain ... switches automatically between 1x and 16x */

  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS); /* 16-bit data but slowest conversions */

  /* Get a new sensor event */
  sensors_event_t event;
  tsl.getEvent(&event);

  if (event.light)
  {
    return event.light;
  }
  else
  {
    return 0.00;
  }
}

int GetInfraredValue(uint16_t *broadband, uint16_t *ir)
{
  Wire.begin(D4, D7);
  /* Initialise the sensor */
  if (!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while (1)
      ;
  }
  /* You can also manually set the gain or enable auto-gain support */
  //tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  //tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true); /* Auto-gain ... switches automatically between 1x and 16x */

  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS); /* 16-bit data but slowest conversions */

  /* Get a new sensor event */
  sensors_event_t event;
  tsl.getEvent(&event);

  if (event.light)
  {
    tsl.getLuminosity(broadband, ir);
    return 0;
  }
  else
  {
    return -1;
  }
}

float GetPressureValue()
{
  Wire.begin(D6, D5);

  while (!bmp.begin())
  {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    delay(2000);
  }
  return bmp.readPressure() / 100.0;
}

float GetTemperatureBMPValue()
{
  Wire.begin(D6, D5);

  while (!bmp.begin())
  {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    delay(2000);
  }
  return bmp.readTemperature();
}

float GetTemperatureValue()
{
  Wire.begin(D2, D1);

  if (!sht31.begin(0x44))
  { // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1)
      delay(1);
  }
  return sht31.readTemperature();
}

float GetHumidityValue()
{
  Wire.begin(D2, D1);

  if (!sht31.begin(0x44))
  { // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1)
      delay(1);
  }
  return sht31.readHumidity();
}

int influxDbUpdate()
{
  InfluxData bmptempRow("temperature_BMP085");
  bmptempRow.addTag("device", chipid);
  float tb = GetTemperatureBMPValue();
  if (isnan(tb))
    return -1;
  bmptempRow.addValue("value", tb);
  influx.prepare(bmptempRow);

  InfluxData tempRow("temperature");
  tempRow.addTag("device", chipid);
  float t = GetTemperatureValue();
  if (isnan(t))
    return -1;
  tempRow.addValue("value", t);
  influx.prepare(tempRow);

  InfluxData humRow("humidity");
  humRow.addTag("device", chipid);
  float h = GetHumidityValue();
  if (isnan(h))
    return -1;
  humRow.addValue("value", h);
  influx.prepare(humRow);

  InfluxData pressRow("pressure");
  pressRow.addTag("device", chipid);
  float p = GetPressureValue();
  if (isnan(p))
    return -1;
  pressRow.addValue("value", p);
  influx.prepare(pressRow);

  InfluxData luxRow("light");
  luxRow.addTag("device", chipid);
  float l = GetLuxValue();
  if (isnan(l))
    return -1;
  luxRow.addValue("value", l);
  influx.prepare(luxRow);

  uint16_t broadband = 0;
  uint16_t infrared = 0;
  GetInfraredValue(&broadband, &infrared);
  if (isnan(broadband) || isnan(infrared))
    return -1;

  InfluxData IrRow("infrared");
  IrRow.addTag("device", chipid);
  IrRow.addValue("value", infrared);
  influx.prepare(IrRow);

  InfluxData VisRow("visible");
  VisRow.addTag("device", chipid);
  VisRow.addValue("value", broadband);
  influx.prepare(VisRow);

  bool ret = influx.write();
  if (ret == false)
  {
    return -1;
  }
  return 0;
}

// ********* timer tick callback ******************
void pushTimerTick()
{
  timerFlag = 1;
}

//************ Start van het programma *************
void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println();
  connectWifi();
  Serial.println();

  Serial.print("Pressure = ");
  Serial.println(GetPressureValue());

  Serial.print("temperature = ");
  Serial.println(GetTemperatureValue());

  Serial.print("humidity = ");
  Serial.println(GetHumidityValue());

  Serial.print("lightlevel = ");
  Serial.print(GetLuxValue());
  Serial.println(" lux");

  uint16_t broadband = 0;
  uint16_t infrared = 0;
  GetInfraredValue(&broadband, &infrared);
  Serial.print("infrared = ");
  Serial.println(infrared);
  Serial.print("visible = ");
  Serial.println(broadband);

  chipid = String(ESP.getChipId()).c_str();
  Serial.print("chipid = ");
  Serial.println(chipid);

  influx.setDb(INFLUXDB_DATABASE);
  //influx.setDbAuth(INFLUXDB_DATABASE, INFLUXDB_USER, INFLUXDB_PASS);

  //Set the update interval timer
  pushTimer.attach(INTERVALTIME, pushTimerTick);
  timerFlag = 1; // stuur meteen de eerste update
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (timerFlag == 1)
    {
      digitalWrite(LED_BUILTIN, LOW);
      if ((influxDbUpdate() != 0))
      {
        //error in server request
        delay(4000); //try after 4 sec
      }
      else
      {
        digitalWrite(LED_BUILTIN, HIGH);
        timerFlag = 0;
      }
    }
  }
  else
  {
    connectWifi();
  }
}
