#include <Multichannel_Gas_GMXXX.h>
#include "Seeed_BME280.h"
#include <SoftwareSerial.h>
#include <Seeed_HM330X.h>
#include <SHT1X.h>
#include <SPI.h>
#include <Wire.h>

GAS_GMXXX<TwoWire> gas;
SoftwareSerial mySerial(10, 11); // RX, TX
BME280 bme280;
SHT1x sht15(A0, A1); //Data, SCK

HM330X sensor;
uint8_t myBuffer[30];
uint8_t myLoRaBuffer[128];
uint8_t tmp[80];

char alphabet[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
// To hexencode the message, and to produce a UUID

const char* str[] = {
  "Check code: %u",
  "PM1.0 concentration (CF=1, Standard particulate matter) %u ug/m3",
  "PM2.5 concentration (CF=1, Standard particulate matter) %u ug/m3",
  "PM10 concentration (CF=1, Standard particulate matter) %u ug/m3",
  "PM1.0 concentration (Atmospheric environment) %u ug/m3",
  "PM2.5 concentration (Atmospheric environment) %u ug/m3",
  "PM10 concentration (Atmospheric environment) %u ug/m3",
  "BME280 Temperature: ",
  "BME280 Humidity: ",
  "SHT15 Temperature: ",
  "SHT15 Humidity: ",
  "Check code: %u",
  "GM102B / NO2:  %u",
  "GM302B / Alcohol gas:  %u",
  "GM502B / VOC gas:  %u",
  "GM702B / CO2:  %u",
};

void hexDump(uint8_t *buf, uint16_t len) {
  String s = "|", t = "| |";
  Serial.println(F("   +.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .A .B .C .D .E .F + +0123456789abcdef+"));
  Serial.println(F("   +------------------------------------------------+ +----------------+"));
  for (uint16_t i = 0; i < len; i += 16) {
    for (uint8_t j = 0; j < 16; j++) {
      if (i + j >= len) {
        s = s + "   "; t = t + " ";
      } else {
        uint8_t c = buf[i + j];
        if (c < 16) s = s + "0";
        s = s + String(c, HEX) + " ";
        if (c < 32 || c > 127) t = t + ".";
        else t = t + (char)c;
      }
    }
    Serial.print((uint8_t)(i / 16), HEX); Serial.print(". "); Serial.println(s + t + "|");
    s = "|"; t = "| |";
  }
  Serial.println(F("   +------------------------------------------------+ +----------------+"));
}

uint8_t px = 0;
void str2hex(uint8_t ix, uint16_t value) {
  uint8_t len;
  memset(tmp, 0, 80);
  if (ix == 0 || ix == 11) {
    memset(myLoRaBuffer, 0, 128);
    px = 0;
    for (uint8_t i = 0; i < 8; i++) {
      tmp[px + i] = alphabet[random(0, 35)];
    }
    sprintf(tmp + 8, ",%u", value);
  } else if (ix < 7 || ix > 10) sprintf(tmp, ",%u", value);
  else {
    String v = "," + String(value / 100.0, 2);
    v.toCharArray(tmp, v.length() + 1);
  }
  len = strlen(tmp);
  for (uint8_t i = 0; i < len; i++) {
    uint8_t x = tmp[i];
    uint8_t y = x >> 4;
    myLoRaBuffer[px++] = alphabet[y];
    myLoRaBuffer[px++] = alphabet[x & 0x0f];
  }
}

HM330XErrorCode print_result(uint8_t ix, uint16_t value) {
  if (NULL == str) {
    return ERROR_PARAM;
  }
  if (ix < 7 || ix > 10) sprintf(tmp, str[ix], value);
  else {
    float v = (float)value / 100.0;
    String x = String((char*)str[ix]) + String(v, 2);
    x.toCharArray(tmp, x.length() + 1);
  }
  Serial.println((char*)tmp);
  str2hex(ix, value);
  return NO_ERROR;
}

/*parse myBuffer with 29 uint8_t-data*/
HM330XErrorCode parse_result(uint8_t* data) {
  uint16_t value = 0;
  uint32_t val = 0;
  if (NULL == data) {
    return ERROR_PARAM;
  }
  for (int i = 1; i < 8; i++) {
    value = (uint16_t) data[i * 2] << 8 | data[i * 2 + 1];
    print_result(i - 1, value);
  }
  print_result(7, (uint16_t)bme280.getTemperature() * 100);
  print_result(8, (uint16_t)bme280.getHumidity() * 100);
  print_result(9, (uint16_t)sht15.readTemperatureC() * 100);
  print_result(10, (uint16_t)sht15.readHumidity() * 100);

  hexDump((char *)myLoRaBuffer, 128);
  mySerial.print("at+txc=1,1000,");
  mySerial.print((char *)myLoRaBuffer);
  mySerial.print("\r\n");
  delay(5000);

  print_result(11, 1);
  val = gas.getGM102B();
  print_result(12, (uint16_t)val);
  val = gas.getGM302B();
  print_result(13, (uint16_t)val);
  val = gas.getGM502B();
  print_result(14, val);
  val = gas.getGM702B();
  print_result(15, (uint16_t)val);
  hexDump((char *)myLoRaBuffer, 128);
  mySerial.print("at+txc=1,1000,");
  mySerial.print((char *)myLoRaBuffer);
  mySerial.print("\r\n");
  return NO_ERROR;
}

HM330XErrorCode parse_result_value(uint8_t* data) {
  if (NULL == data) {
    return ERROR_PARAM;
  }
  uint8_t sum = 0;
  for (int i = 0; i < 28; i++) {
    sum += data[i];
  }
  if (sum != data[28]) {
    Serial.println("wrong checkSum!!!!");
  }
  Serial.println("");
  return NO_ERROR;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) ;
  Serial.print(F("HM330X init "));
  if (sensor.init()) {
    Serial.println(F("[x]\n Stopping here: nothing else to do..."));
    while (1);
  }
  Serial.println(F("[o]"));
  Serial.print(F("BME280 init "));
  if (!bme280.init()) {
    Serial.println(F("[x]\n Stopping here: nothing else to do..."));
  }
  Serial.println(F("[o]"));
  Serial.print(F("GAS_GMXXX init "));
  gas.begin(Wire, 0x08); // use the hardware I2C
  Serial.println(F("[o]"));

  mySerial.begin(115200);
  Serial.print("at+version\r\n");
  mySerial.print("at+version\r\n");
  delay(1000);
  while (mySerial.available()) Serial.write(mySerial.read());
  Serial.write('\n');
  randomSeed(analogRead(0));
}

void loop() {
  if (sensor.read_sensor_value(myBuffer, 29)) {
    Serial.println("HM330X read result failed!!!");
  } else {
    parse_result_value(myBuffer);
    parse_result(myBuffer);
    Serial.println("");
  }
  delay(10000);
}
