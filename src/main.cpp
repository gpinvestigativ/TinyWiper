#include <Arduino.h>
#include <vector>

#include "SdFat.h"
#include <sdios.h>
#include "FreeStack.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "data.h"
#include <trng.h>

//#define DEBUG_BATT

#define VERSION 0.2

const float analogDivider = 160.0f;

#define cardDetectPin 23
#define writeProtectPin 22
#define DISPLAY_RST_PIN 15

const size_t BUF_SIZE = 512;

#define SD_CONFIG SdioConfig(FIFO_SDIO)

//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t* buf = (uint8_t*)buf32;

SdioCard sd;

Adafruit_SSD1306 display(128, 64, &Wire, DISPLAY_RST_PIN, 4000000UL, 100000UL);

ArduinoOutStream cout(Serial);

void cidDmp() {
  cid_t cid;
  if (!sd.readCID(&cid)) {
    cout << "CID Error" << endl;
  }
  cout << F("\nManufacturer ID: ");
  cout << hex << int(cid.mid) << dec << endl;
  cout << F("OEM ID: ") << cid.oid[0] << cid.oid[1] << endl;
  cout << F("Product: ");
  for (uint8_t i = 0; i < 5; i++) {
    cout << cid.pnm[i];
  }
  cout << F("\nVersion: ");
  cout << int(cid.prv_n) << '.' << int(cid.prv_m) << endl;
  cout << F("Serial number: ") << hex << cid.psn << dec << endl;
  cout << F("Manufacturing date: ");
  cout << int(cid.mdt_month) << '/';
  cout << (2000 + cid.mdt_year_low + 10 * cid.mdt_year_high) << endl;
  cout << endl;
}

void drawBattery(int offX = 0, int offY = 0) {
  static int battLutOffset = 540;
  int aread0 = analogRead(0);

  #ifdef DEBUG_BATT
  cout << F("Aread 0: ") << aread0 << endl;
  #endif

  int battLutIdx = aread0 - battLutOffset;
  battLutIdx = constrain(battLutIdx, 0, 130);
  int battPercent = battLut[battLutIdx];
  int battBarValue = round((float)battPercent / 100.0f * 11);
  display.drawRect(offX, offY, 12, 7, SSD1306_WHITE);
  display.fillRect(offX, offY, battBarValue, 7, SSD1306_WHITE);
  display.drawLine(offX + 12, offY + 2, offX + 12, offY + 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(offX + 17, offY);
  display.setTextColor(SSD1306_WHITE);
  display.print(battPercent); display.print(" %");
}

void showStartScreen() {
  static int aniCount = 0;
  display.clearDisplay();
  display.drawBitmap(128 - (aniCount > 35 ? 35 : aniCount), 1, sdCardBitmap, 77, 62, SSD1306_WHITE);
  aniCount += 1;
  if (aniCount > 50) aniCount = 0;
  drawBattery();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 15); display.print("Insert");
  display.setCursor(0, 35); display.print("SD Card");
  display.display();
}

//------------------------------------------------------------------------------
void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(2);
  display.clearDisplay();
  display.drawBitmap(0, 0, teensyWiper01Bitmap, 128, 64, SSD1306_WHITE);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(105, 49);
  display.setTextSize(1);
  display.print(VERSION,1);
  display.display();

  pinMode(cardDetectPin, INPUT_PULLUP);
  pinMode(writeProtectPin, INPUT_PULLUP);

  Serial.begin(115200);
  //while (!Serial);

  // use uppercase in hex and use 0X base prefix
  cout << uppercase << showbase << endl;

  trng_init();
  uint32_t tword = trng_word();
  cout << "random seed: " << tword << endl;
  srandom(tword);
  delay(1000);
}
//------------------------------------------------------------------------------
void loop() {
  uint32_t t = 0;
  int durationMinutes = 0;
  bool writable = false;
  bool randWriteCompleted = false;
  bool formatCompleted = false;

  //wait for card insert
  while (digitalRead(cardDetectPin) == 1) {
    showStartScreen();
    delay(50);
  }
  delay(100);

  #ifdef WRITEPROTECTCHECK
  if (digitalRead(writeProtectPin) == 0) {
    delay(500);
    if (digitalRead(writeProtectPin) == 0)
      writable = true;
    else
      writable = false;

  } else {
    writable = false;
  }
  #else
  writable = true;
  #endif


  while (!sd.begin(SD_CONFIG)) {
    if (digitalRead(cardDetectPin) == 1) break;
  }

  cout << F("Card size: ") << sd.sectorCount() * 512E-9;
  cout << F(" GB (GB = 1E9 bytes)") << endl;
  cout << F("Speed: ") << sd.kHzSdClk() << F(" kHz") << endl;
  cout << F("sectorCount: ") << sd.sectorCount() << endl;

  cidDmp();

  //pre wipe info screen
  int aniCount = 64;
  while (aniCount > 0) {
    display.clearDisplay();
    drawBattery();
    display.drawBitmap(100, 1, sdCardBitmap, 77, 62, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 15);
    display.setTextSize(2);
    display.print(sd.sectorCount() * 512E-9); display.println(" GB");
    display.setTextSize(1);
    display.print(sd.kHzSdClk() / 1000); display.println(" MHz");
    if (writable)
    {
      display.setCursor(0, 55);
      display.println("Starting Wipe...");
      display.drawLine(0, 63, 128 - aniCount * 2, 63, SSD1306_WHITE);
      aniCount--;
    } else {
      display.setCursor(0, 56);
      display.println("SD Card locked");
    }
    display.display();
    delay(30);
    if (digitalRead(cardDetectPin) == 1) break;
  }

  //actual wiping
  int failedSectorCount = 0;
  std::vector<unsigned int> failedSectors;
  #if 1
  if (digitalRead(cardDetectPin) == 0) {
    cout << F("Starting full disk wipe") << endl << endl;
    t = millis();
    uint32_t lastT = t;
    for (unsigned int i = 0; i < sd.sectorCount(); i++) {
      if (i % 10 == 0) {
        for (size_t i = 0; i < BUF_SIZE/4; i++) {
          buf32[i]= random();
        }
      }
      auto startus = micros();
      bool writeStatus = sd.writeSector(i, buf);

      if(!writeStatus){ //error handling
        cout << (int)sd.errorCode() << " ";
        printSdErrorSymbol(&Serial, sd.errorCode());
        cout << " ";
        printSdErrorText(&Serial, sd.errorCode());
        cout << endl;
        failedSectorCount++;
        failedSectors.push_back(i);
      }
      if(micros()-startus > 888888){ //hacky fix for random SD Card errors
        cout << "restart sd " << i << endl;
        sd.begin(SD_CONFIG);
      }

      if ((millis() - lastT) > 1000) {
        lastT = millis();
        uint32_t usedtime = millis() - t;
        double msPerSector = (double)usedtime / (double)i;
        uint32_t sectorsLeft = sd.sectorCount() - i;

        //cout << ((double)i/(double)sd.sectorCount())*100 << " % - " << i*512E-3/usedtime << " MB/s - " << usedtime << " " << msPerSector << " " << sectorsLeft << " " << (sectorsLeft*msPerSector)/60000 << " min left"<< endl;

        display.clearDisplay();
        drawBattery();
        display.drawBitmap(100, 1, sdCardBitmap, 77, 62, SSD1306_WHITE);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 15);
        display.setTextSize(2);
        double progress = ((double)i / (double)sd.sectorCount()) * 100;
        display.print(progress); display.println(" %");
        display.setTextSize(1);
        display.print(i * 512E-3 / usedtime); display.println(" MB/s");
        display.print((int)((sectorsLeft * msPerSector) / 60000)); display.println(" min left");
        display.drawRect(0, 52, 80, 8, SSD1306_WHITE);
        display.fillRect(2, 54, (int)round(progress * 0.76), 4, SSD1306_WHITE);
        display.display();
        if (digitalRead(cardDetectPin) == 1) break;
      }
      if (digitalRead(cardDetectPin) == 1) break;
      if (i == sd.sectorCount() - 1)
        randWriteCompleted = true;
    }
  }
  #endif

  //retry failed Sectors
  #if 1
  //for(int x = 0; x < 8000; x++)failedSectors.push_back(x);

  cout << "retry " << failedSectors.size() << " failed sectors";
  uint32_t lastT = t;
  uint32_t curSector = 0;
  for(auto sec : failedSectors){
    int secRetries = 5;
    curSector++;
    for (size_t i = 0; i < BUF_SIZE; i += 4) {
      buf[i] = random();
    }
    while(secRetries-- > 0){
      auto startus = micros();
      if(sd.writeSector(sec, buf)){
        secRetries = 0;
        failedSectorCount--;
      }
      if(micros()-startus > 888888){ //hacky fix for random SD Card errors
        cout << "restart sd " << sec << endl;
        sd.begin(SD_CONFIG);
      }
    }

    if ((millis() - lastT) > 200) {
      lastT = millis();
      uint32_t usedtime = millis() - t;
      double msPerSector = (double)usedtime / (double)curSector;
      uint32_t sectorsLeft = failedSectors.size() - curSector;

      //cout << ((double)i/(double)sd.sectorCount())*100 << " % - " << i*512E-3/usedtime << " MB/s - " << usedtime << " " << msPerSector << " " << sectorsLeft << " " << (sectorsLeft*msPerSector)/60000 << " min left"<< endl;

      display.clearDisplay();
      drawBattery();
      display.drawBitmap(100, 1, sdCardBitmap, 77, 62, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.println("Retry failed");
      display.println("sectors...");
      double progress = ((double)curSector / (double)failedSectors.size()) * 100;
      //display.print(curSector * 512E-3 / usedtime); display.println(" MB/s");
      display.print(progress); display.println(" %");
      //display.print((int)((sectorsLeft * msPerSector) / 60000)); display.println(" min left");
      display.drawRect(0, 52, 80, 8, SSD1306_WHITE);
      display.fillRect(2, 54, (int)round(progress * 0.76), 4, SSD1306_WHITE);
      display.display();
      if (digitalRead(cardDetectPin) == 1) break;
    }
  }
  cout << "finished, " << failedSectorCount << " sectors couldn't be deleted";
  #endif

  //FORMAT SD Card after random data write
  // SdCardFactory constructs and initializes the appropriate card.
  //#define FORMATCARD
  #ifdef FORMATCARD
  if (digitalRead(cardDetectPin) == 0) {
    int formatTries = 10;
    while(formatTries-- > 0 && !formatCompleted){
      formatCompleted = true;
      SdCardFactory cardFactory;
      SdCard* m_card = cardFactory.newCard(SD_CONFIG);
      if (!m_card || m_card->errorCode()) {
        cout << "card init failed" << endl;
        formatCompleted = false;
      }
      uint8_t  sectorBuffer[512];
      FatFormatter fatFormatter;
      if (!fatFormatter.format(m_card, sectorBuffer, &Serial)) {
        cout << "card format failed" << endl;
        formatCompleted = false;
      }
    }
  }
  #endif

  t = millis() - t;
  durationMinutes = (int)round((t / 60000));
  cout << endl << F("Done") << endl;

  float successRate = ((float)(sd.sectorCount()-failedSectorCount)/(float)sd.sectorCount())*100.0f;

  cout << sd.sectorCount() << " f: " << failedSectorCount << endl;

  //wait for card disconnect
  aniCount = 28;
  int cardDetectCounter = 0;
  while (cardDetectCounter < 10) {
    if(!digitalRead(cardDetectPin)){
      cardDetectCounter = 0;
    }else{
      cardDetectCounter++;
    }
    display.clearDisplay();
    display.drawBitmap(128 - aniCount, 1, sdCardBitmap, 77, 62, SSD1306_WHITE);
    aniCount -= 1;
    if (aniCount < -20) aniCount = 28;
    drawBattery();
    display.setTextColor(SSD1306_WHITE);

    if (randWriteCompleted) {
      display.setTextSize(2);
      display.setCursor(0, 15); display.print("Finished");
      display.setTextSize(1);display.setTextSize(1);
      display.setCursor(0, 35); display.print(durationMinutes); display.println(" min");
      display.setCursor(0, 46); display.print(successRate); 
      if(formatCompleted)
        display.println(" % success");
      else
        display.println(" % success.");
    } else if (!randWriteCompleted && formatCompleted) {
      display.setTextSize(2);
      display.setCursor(0, 15); display.print("Error");
      display.setTextSize(1);
      display.setCursor(0, 35); display.print("Wipe failed");
    } else {
      display.setTextSize(2);
      display.setCursor(0, 15); display.print("Error");
      display.setTextSize(1);
      display.setCursor(0, 35); display.print("all failed");
    }
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.println("remove SD Card");
    display.display();
    delay(50);
  }
}

