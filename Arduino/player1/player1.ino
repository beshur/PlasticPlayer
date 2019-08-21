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
#include <Adafruit_SSD1306.h>
#include <splash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SCREEN
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };


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


// MISC TIMERS
unsigned long startMillis;
unsigned long currentMillis;

// SERIAL Terminator
const char serialTerminator = 23;
String serialTmp;

// BUTTONS
const int playButtonPin = 2;

// STATUS LED
const int statusLedR = 3;
const int statusLedG = 6;
const int statusLedB = 5;

// ENCODER
const int encoderInputA = 8;
const int encoderInputB = 9;

void setup(void) {
  Serial.begin(115200);
  Serial.println("DI Player 1.0");
  setupStatusLed();
  setupButton();
  startMillis = millis();
  nfc.begin();

  // Serial.println("NFC began");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    // Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

}

void loop(void) {
  currentMillis = millis();
  if (currentMillis - startMillis > nfcDelay) {
    scanNfc();
    startMillis = currentMillis;
  }
  listenComputer();
}

void setupStatusLed(void) {
  pinMode(statusLedR, OUTPUT);
  pinMode(statusLedG, OUTPUT);
  pinMode(statusLedB, OUTPUT);

  changeStatusLedColor("yellow");
}

void changeStatusLedColor(String color) {
  if (color.equals("green")) {
    // arduino ready
    analogWrite(statusLedR, 0);
    analogWrite(statusLedG, 120);
    analogWrite(statusLedB, 0);
  } else if (color.equals("white")) {
    // handshake over serial
    analogWrite(statusLedR, 120);
    analogWrite(statusLedG, 120);
    analogWrite(statusLedB, 120);
  } else if (color.equals("yellow")) {
    // init
    analogWrite(statusLedR, 120);
    analogWrite(statusLedG, 120);
    analogWrite(statusLedB, 0);
  }
}


void setupEncoder(void) {
  pinMode(encoderInputA, INPUT);
  pinMode(encoderInputB, INPUT);
}

void setupButton() {
  attachInterrupt(digitalPinToInterrupt(playButtonPin), readPlayButton, RISING);
}

void readPlayButton() {
  msgComputer("button&play");
}

void scanNfc() {
  nfcPtrTmp = "";
  nfcCardId = "";
  nfcCardWifi = "";
  if (!nfc.tagPresent()) {
    msgComputer("button&clear");
  } else {
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

    if (nfcCardWifi.length()) {
      drawText("WI-FI Card");
      msgComputer(nfcCardWifi);
    } else {
      drawText("Loading...");
      msgComputer(formatNfcCardId(nfcCardId));
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

  char *ptr = strtok(strCopy.c_str(), delim);

  for (int i = 0; i < init_size; i++)
  {
    if (i == 0) {
      serialTmp = String(ptr);
    }
    ptr = strtok(NULL, delim);

    msgComputer("inside listenComputer serialTmp: " + serialTmp + String(ptr));
    if (serialTmp.equals("text")) {
      drawText(String(ptr));
      break;
    }

    if (ptr == NULL) {
      break;
    }
  }
}

/* graphics */
void drawText(String text) {
  display.clearDisplay();

  display.setTextWrap(false);
//  drawScrolltext(text);
//  return;
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();      // Show initial text
  delay(100);
}

void drawScrolltext(String text) {
  display.clearDisplay();

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();      // Show initial text
  delay(100);
  scrollTextLeftRight();
}

void scrollTextLeftRight(void) {
  // Scroll in various directions, pausing in-between:
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  scrollTextLeftRight();
}
