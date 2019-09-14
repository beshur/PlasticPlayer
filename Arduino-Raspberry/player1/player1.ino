/*
  Plastic Player Arduino Controller

  Alex Buznik <shu@buznik.net>
  2019
*/
#include <SPI.h>
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SCREEN
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
String screenText = "";
String screenTitle = "";
String screenSys = "";
const int textDelay = 2000;
int titleTextI = 0;
byte titleTextFwd = 1;
bool textTimeElapsed = true;

const String VERSION = "1.0.0";

// NFC
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
/* Uno's A4 to SDA & A5 to SCL */
const int nfcDelay = 1000;
String nfcCardId = "";
String nfcCardWifi = "";
String nfcPtrTmp = "";
bool oldNfcCardPresent = false;

// HANDSHAKE CONFIRM
bool handshake = false;

// MISC TIMERS
unsigned long nfcStartMillis;
unsigned long currentMillis;
unsigned long textMillis;
unsigned long titleTextMillis;

// SERIAL Terminator
const char serialTerminator = 23;

// BUTTONS
const int playButtonPin = 2;
volatile byte playButtonInit = LOW;
const int playButtonDebounce = 200;
unsigned long playButtonDebounceMs = 0;

// STATUS LED
const int statusLedR = 9;
const int statusLedG = 10;
const int statusLedB = 11;

// VOLUME
const int volumePotPin = 0;
int oldVolume = 0;
int oldVolumeAvg = 0;
byte volumeSamplerI = 0;
const int volumeSamplesCount = 1;
int volumeSamples[volumeSamplesCount];

void setup(void) {
  Serial.begin(115200);
  Serial.setTimeout(300);
  msgComputer("DI Player " + VERSION);
  msgComputer("handshake&");
  setupStatusLed();
  setupButtons();
  setupVolumePot();
  currentMillis = millis();
  nfcStartMillis = currentMillis;
  textMillis = currentMillis;
  titleTextMillis = currentMillis;
  nfc.begin();

  msgComputer("NFC began");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    msgComputer("error&SSD1306 allocation failed");
    changeStatusLedColor("red");
    for(;;); // Don't proceed, loop forever
  } else {
    setTextSettings();
    drawText("DI Player");
  }
}

void loop(void) {
  currentMillis = millis();
  if (currentMillis - nfcStartMillis > nfcDelay) {
    nfcStartMillis = currentMillis;
    scanNfc();
  } else {
    // readVolumePot();
  }
  checkPlayButton();
  renderScreenState();
  readSerial();
}

void setupVolumePot() {
  for (int i = 0; i < volumeSamplesCount; i++) {
    volumeSamples[i] = 0;
  }
}

void setupStatusLed(void) {
  pinMode(statusLedR, OUTPUT);
  pinMode(statusLedG, OUTPUT);
  pinMode(statusLedB, OUTPUT);

  changeStatusLedColor("yellow");
}

void changeStatusLedColor(String color) {
  int brightness = 50;
  if (color.equals("green")) {
    // arduino ready
    analogWrite(statusLedR, 0);
    analogWrite(statusLedG, brightness);
    analogWrite(statusLedB, 0);
  } else if (color.equals("white")) {
    // handshake over serial
    analogWrite(statusLedR, brightness);
    analogWrite(statusLedG, brightness);
    analogWrite(statusLedB, brightness);
  } else if (color.equals("yellow")) {
    // init
    analogWrite(statusLedR, brightness);
    analogWrite(statusLedG, brightness);
    analogWrite(statusLedB, 0);
  } else if (color.equals("blue")) {
    analogWrite(statusLedR, 0);
    analogWrite(statusLedG, 0);
    analogWrite(statusLedB, brightness);
  } else if (color.equals("red")) {
    analogWrite(statusLedR, brightness);
    analogWrite(statusLedG, 0);
    analogWrite(statusLedB, 0);
  }
}


void readVolumePot(void) {
  // This is a simple smoothing algo
  // Change volumeSamplesCount to improve results
  // I got just got a good potentiometer, and don't need it
  int newVolume = analogRead(volumePotPin);
  oldVolume = oldVolume - volumeSamples[volumeSamplerI];
  volumeSamples[volumeSamplerI] = newVolume;

  oldVolume = oldVolume + newVolume;
  int volumeAvg = min(oldVolume/volumeSamplesCount/10, 100);
  volumeSamplerI = volumeSamplerI + 1;
  if (volumeSamplerI >= volumeSamplesCount) {
    volumeSamplerI = 0;
  }

  if (abs(oldVolumeAvg - volumeAvg) > 4) {
    oldVolumeAvg = volumeAvg;
    msgComputer("volume&" + String(volumeAvg));
    screenText = "Volume: " + String(volumeAvg) + "%";
  }
}

void setupButtons() {
  pinMode(playButtonPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(playButtonPin), onPlayButton, HIGH);
}

void onPlayButton() {
  playButtonInit = HIGH;
}

void checkPlayButton() {
  if (playButtonInit == HIGH) {
    if (currentMillis - playButtonDebounceMs > playButtonDebounce) {
      onPlayButtonAction();
    }
  }
}

void onPlayButtonAction() {
  playButtonDebounceMs = currentMillis;
  playButtonInit = LOW;
  msgComputer("button&play");
}

void scanNfc() {
  nfcCardId = "";
  nfcCardWifi = "";
  nfcPtrTmp = "";
  if (nfc.tagPresent(50)) {
    NfcTag tag = nfc.read();
    nfcCardId = tag.getUidString();

    if (tag.hasNdefMessage())
    {
      NdefMessage message = tag.getNdefMessage();
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++)
      {
        NdefRecord record = message.getRecord(i);

        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);

         // Processes the message as a string vs as a HEX value
        String payloadAsString = "";
        for (int c = 0; c < payloadLength; c++)
        {
          payloadAsString += (char)payload[c];
        }

         // Detect WI-FI
        String strCopy = payloadAsString;
        char delim[] = "&";
        char *ptr = strtok(strCopy.c_str(), delim);

        nfcPtrTmp = String(ptr);

        // it's a wifi card!
        if (nfcPtrTmp.equals("wifi")) {
          nfcCardWifi = payloadAsString;
          break;
        }
      }
    }
  }

  if (nfcCardId == "") {
    if (oldNfcCardPresent == true) {
      // card removed
      oldNfcCardPresent = false;
      msgComputer("button&clear");
    }
  } else {
    if (oldNfcCardPresent == false) {
      // card deployed
      oldNfcCardPresent = true;

      if (nfcCardWifi.length() > 0) {
        msgComputer(nfcCardWifi);
        screenSys = "Setting Wi-Fi";
      } else {
        msgComputer(formatNfcCardId(nfcCardId));
        screenTitle = nfcCardId;
      }
    }
  }
}

String formatNfcCardId(String id) {
  return "play&" + id;
}

void msgComputer(String data) {
  Serial.println(data + serialTerminator);
}

void readSerial() {
  String read = Serial.readStringUntil(serialTerminator);
  String message = "";
  String prefix = "";

  // understand the received data
  int init_size = read.length();
  String strCopy = read;
  char delim[] = "&";
  if (init_size > 0) {
    char *ptr = strtok(strCopy.c_str(), delim);
    prefix = String(ptr);

    ptr = strtok(NULL, delim);
    message = String(ptr);

    if (prefix.equals("text")) {
      screenText = message;
    } else if (prefix.equals("sys")) {
      screenSys = message;
    } else if (prefix.equals("title")) {
      screenTitle = message;
    } else if (prefix.equals("handshake")) {
      handshake = true;
      screenText = "Ready";
      changeStatusLedColor("white");
    }
  }
}

/* graphics */
void renderScreenState() {
  if (currentMillis - textMillis > textDelay) {
    textTimeElapsed = true;
  }

  if (screenSys.length() > 0) {
    drawTextScroll(screenSys);
  } else if (oldNfcCardPresent == true) {
    if (screenText.length() > 0) {
      drawText(screenText);
      textMillis = currentMillis;
      textTimeElapsed = false;
      screenText = "";
    } else if (screenTitle.length() > 0 && textTimeElapsed == true) {
      drawTextScroll(screenTitle);
    } else {
      // Keeping the previous screenText
    }
  } else {
    // default when card not present
    drawText("Idle");
  }
}

void setTextSettings() {
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 3);
}

void drawText(String text) {
  display.clearDisplay();
  display.setCursor(0, 3);
  display.println(text);
  display.display();
}

void drawTextScroll(String text) {
  String temp = text.substring(titleTextI, titleTextI + 10);

  if (currentMillis - titleTextMillis < 300) {
    return;
  } else {
    titleTextMillis = currentMillis;
  }

  if (titleTextI <= 0 && titleTextFwd != 1) {
    // reached the beginning
    titleTextFwd = 1;
    titleTextI = 0;
  } else if (temp.length() < 10 || titleTextFwd == 0) {
    // reached the end
    titleTextI--;
    titleTextFwd = 0;
  } else {
    // go forward
    titleTextI++;
  }

  drawText(temp);
}
