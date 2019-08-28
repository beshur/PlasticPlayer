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

const String VERSION = "1.0.0";

// NFC
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
/* Uno's A4 to SDA & A5 to SCL */

/* in order to detect wifi cards,
 * they should have the following format:
 * format: text/plain
 * data: wifi&[ssid name]&[password]
 */
int nfcDelay = 1000;
String nfcPtrTmp;
String nfcCardId;
String nfcCardWifi;
int oldNfcCardPresent = 0;

// HANDSHAKE CONFIRM
int handshake = 0;

// MISC TIMERS
unsigned long startMillis;
unsigned long currentMillis;

// SERIAL Terminator
const char serialTerminator = 23;
String serialTmp;

// BUTTONS
const int playButtonPin = 5;
int playButtonInit = 0;
const int nextButtonPin = 7;
int nextButtonInit = 0;

// STATUS LED
const int statusLedR = 9;
const int statusLedG = 10;
const int statusLedB = 11;

// VOLUME
const int volumePotPin = 2;
int oldVolume = 0;

void setup(void) {
  Serial.begin(115200);
  Serial.setTimeout(300);
  Serial.println("DI Player " + VERSION);
  Serial.println("handshake&");
  setupStatusLed();
  setupButtons();
  startMillis = millis();
  nfc.begin();

  Serial.println("NFC began");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println("error&SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  } else {
    drawText("DI Player");
  }
}

void loop(void) {
  currentMillis = millis();
  if (currentMillis - startMillis > nfcDelay) {
    scanNfc();
    startMillis = currentMillis;
  }
  readVolumePot();
  listenComputer();
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
  }
}


void readVolumePot(void) {
  int newVolume = analogRead(volumePotPin);
  if (newVolume != oldVolume) {
    String volume = String(newVolume/10);
    msgComputer("volume&" + volume);

    oldVolume = newVolume;
  }
}

void setupButtons() {
  pinMode(playButtonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(playButtonPin), onPlayButton, CHANGE);

}

void onNextButton() {
  if (nextButtonInit == 0) {
    nextButtonInit = 1;
  } else {
    msgComputer("button&next");
  }
}

void onPlayButton() {
  if (playButtonInit == 0) {
    playButtonInit = 1;
  } else {
    msgComputer("button&play");
  }
}

void scanNfc() {
  nfcPtrTmp = "";
  nfcCardId = "";
  nfcCardWifi = "";
  if (nfc.tagPresent(50)) {
    NfcTag tag = nfc.read();
    nfcCardId = tag.getUidString();

    if (tag.hasNdefMessage())
    {
      NdefMessage message = tag.getNdefMessage();

      // If you have more than 1 Message then it wil cycle through them
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
        int init_size = payloadLength;
        String strCopy = payloadAsString;
        char delim[] = "&";
        nfcCardWifi = "";

        char *ptr = strtok(strCopy.c_str(), delim);

        for (int i = 0; i < init_size; i++)
        {
          nfcPtrTmp = String(ptr);
          ptr = strtok(NULL, delim);

          // it's a wifi card!
          if (nfcPtrTmp.equals("wifi")) {
            nfcCardWifi = payloadAsString;
            break;
          }

          if (ptr == NULL) {
            break;
          }
        }
      }

    }
  }
  if (nfcCardId == "") {
    if (oldNfcCardPresent == 1) {
      // card removed
      oldNfcCardPresent = 0;
      msgComputer("button&clear");
    }
  } else {
    if (oldNfcCardPresent == 0) {
      // card deployed
      oldNfcCardPresent = 1;
      if (nfcCardWifi.length()) {
        drawText("WI-FI Card");
        msgComputer(nfcCardWifi);
      } else {
        drawText("Loading...");
        msgComputer(formatNfcCardId(nfcCardId));
      }
    }
  }
}

String formatNfcCardId(String id) {
  return "play&" + id;
}

void msgComputer(String data) {
  Serial.println(data);
}

void listenComputer() {
  String read = Serial.readStringUntil(serialTerminator);

  // understand the received data
  int init_size = read.length();
  String strCopy = read;
  char delim[] = "&";
  if (init_size > 0) {
    char *ptr = strtok(strCopy.c_str(), delim);

    for (int i = 0; i < init_size; i++)
    {
      if (i == 0) {
        serialTmp = String(ptr);
      }
      ptr = strtok(NULL, delim);

      if (serialTmp.equals("text")) {
        drawText(String(ptr));
        break;
      } else if (serialTmp.equals("handshake")) {
        drawText("Ready");
        changeStatusLedColor("white");
        break;
      }

      if (ptr == NULL) {
        break;
      }
    }
  }
}

/* graphics */
void drawText(String text) {
  display.clearDisplay();

  display.setTextWrap(false);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
}
