# Uno + RAK811 Shield (aka WisNode LoRa) Example

## Introduction

This sketch shows how to use a [WisNode LoRa](https://store.rakwireless.com/products/rak811-lpwan-evaluation-board) with an Arduino Uno or similar to send data from various environmental sensors as LoRa packets. The data is picked up (as an example) by another RAK811 connected to a PC with Python code. See [companion project](https://github.com/Kongduino/DecodeWisnode). In this case I'm using the following sensors:

1. A [BME280 sensor](https://www.seeedstudio.com/Grove-BME280-Environmental-Sensor-Temperature-Humidity-Barometer.html), from SeeedStudio. I'm using [their library](https://github.com/Seeed-Studio/Grove_BME280).
2. An [HM3301 Laser PM2.5 Dust Sensor](https://www.seeedstudio.com/Grove-Laser-PM2-5-Sensor-HM3301.html), again from Seeed. Using [their library](https://github.com/Seeed-Studio/Seeed_PM2_5_sensor_HM3301).
3. An [SHT15 Temperature + Humidity sensor](https://item.taobao.com/item.htm?spm=a1z09.2.0.0.75e72e8d5tdtmv&id=611658565292&_u=a250qoit0372) from Taobao. Using the [SHT1x sensor library for ESPx](https://github.com/practicalarduino/SHT1x).
4. A [Multichannel Gas Sensor](https://wiki.seeedstudio.com/Grove-Multichannel-Gas-Sensor-V2/) again from SeeedStudio. They provide [a library](https://github.com/Seeed-Studio/Seeed_Arduino_MultiGas).

## Operation

The WisNode LoRa's firmware is AT based, which means that while it can be convenient to use, there are some limitations. The main one being that strings have to be hex-encoded: you don't send `01234`, you send `3031323334`. Also, there's a limitation on the length of the string you can send. Since we're sending quite a bit of data, we will send two strings instead of one, making it easier on ourselves:

1. BME280, HM3301 and SHT15.
2. The Multichannel Gas Sensor (4 data points).

In the Serial consile, we'll display the data, plus whatever we're sending, in a fex dump that makes it easier to read and debug. I wrote a quick `hexDump` routine that does the job nicely:

```c
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
```

Similarly, the `str2hex` function encodes the string we want to send into an hex string:

```c
void str2hex(uint8_t len) {
  uint8_t px = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t x = tmp[i];
    uint8_t y = x >> 4;
    myLoRaBuffer[px++] = alphabet[y];
    myLoRaBuffer[px++] = alphabet[x & 0x0f];
  }
}
```
We put the encoded string directly into the `myLoRaBuffer` char array that will be used to send the string.

```c
void encodeSend() {
  str2hex(strlen(tmp));
  hexDump((char *)myLoRaBuffer, 128);
  mySerial.print("at+txc=1,1000,");
  mySerial.print((char *)myLoRaBuffer);
  mySerial.print("\r\n");
}
```

I am using here an older version of the firmware, which can be recognized from the send comma: `at+txc=1,1000,`: it tells the WisNode LoRa to send the string `1` time, every `1000` milliseconds. The old firmware allowed you to send repeatedly the same string at an interval, without having to do it every time. The new firmware removed that. The new command is `at+send=lorap2p:<data>`. In this case the reppetition is not needed and superfluous, but we could imagine a scenario where we needed that.

We start the string with a UUID: we want to make sure the LoRa receiver(s) know whether this data stream is a repeat or a new one.

```c
char alphabet[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
// To hex-encode the message, and to produce a UUID

[...]

  memset(myLoRaBuffer, 0, 128);
  uint8_t px = 0;
  for (uint8_t i = 0; i < 8; i++) {
    tmp[px + i] = alphabet[random(0, 35)];
  }
  px = strlen(tmp);
```

Note that in the setup, we do not initialize the WisNode LoRa: since it saves its settings, and reloads them at boot, we don't need to do that. However, using the old firmware I set it up with: `at+rf_config=433125000,SF,BW,CR,PREAMBLE,POWER`. In `loop()` we call repeatedly the two functions that produce the data and send it, and wait 5 seconds.










