/*

* PN532 NFC RFID Module (v3)

* NFC Tag Quick Test Code v1

* Based on an example code from PN532 Arduino Library

* Tailored for I2C & Arduino Uno

* Remember to set mode switch of PN532 module to I2C!

* T.K.Hareendran/2019

* www.electroschematics.com

*/

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
/* Uno's A4 to SDA & A5 to SCL */

unsigned long startMillis;
unsigned long currentMillis;

/* in order to detect wifi cards,
 * they should have the following format:
 * format: text/plain
 * data: wifi&[ssid name]&[password]
 */
int nfcDelay = 1000;
String nfcPtrTmp;
String nfcCardId;
String nfcCardWifi;

void setup(void) {
  Serial.begin(9600);
  Serial.println("DI Player 1");
  startMillis = millis();
  nfc.begin();
}

void loop(void) {
  scanNfc(); 
  delay(nfcDelay);
}

void scanNfc() {
  Serial.println("\nScan a NFC tag\n");
  nfcPtrTmp = "";
  nfcCardId = "";
  nfcCardWifi = "";
  if (nfc.tagPresent())
  {
    NfcTag tag = nfc.read();
    tag.print();
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
        
        String payloadAsString = ""; // Processes the message as a string vs as a HEX value
        for (int c = 0; c < payloadLength; c++) 
        {
          payloadAsString += (char)payload[c];
        }
        
        Serial.println("\nTag Content Shown Below\n");
        
        Serial.print("Original String: ");
        Serial.println(payloadAsString);

        // Detect WI-FI
        int init_size = payloadLength;
        String strCopy = payloadAsString;
        char delim[] = "&";
        nfcCardWifi = "";
      
        char *ptr = strtok(strCopy.c_str(), delim);
      
        for (int i = 0; i < init_size; i++)
        {
          nfcPtrTmp = String(ptr);
          Serial.println("token: " + nfcPtrTmp);
          Serial.println("token is wifi: " + nfcPtrTmp.equals("wifi"));
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

        String uid = record.getId();
        if (uid != "") 
        {
          Serial.print("  ID: ");
          Serial.println(uid);
        }
      }
      
    }
  }

  if (nfcCardWifi.length()) {
    msgComputer(nfcCardWifi);
  } else {
    msgComputer(nfcCardId);
  } 
}

void msgComputer(String data) {
  Serial.println("msgComputer");
  Serial.println(data);
}

