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
#define FORMATCARD

#define VERSION 0.2

const size_t BUF_SIZE = 512;

#define SD_CONFIG SdioConfig(FIFO_SDIO)

#define LED 13

//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t *buf = (uint8_t *)buf32;

SdioCard sd;
ArduinoOutStream cout(Serial);

IntervalTimer ledTimer1;
IntervalTimer ledTimer2;
IntervalTimer ledTimer3;
IntervalTimer ledTimer4;

double progress = 0.0f;
uint32_t blinkDutyMicros = 1500000;
uint32_t errorDutyMicros = 50000;
uint32_t finishedDutyMicros = 100000;

void ledOff(){
  analogWrite(LED, 0);
  ledTimer3.end();
}

void blinkLed(){
  ledTimer3.begin(ledOff, constrain((progress/100.0f)*blinkDutyMicros,50000, blinkDutyMicros));
  analogWrite(LED, 255);
}

void fadeLed(){
  static uint8_t ledVal = 0;
  static int8_t ledAdder = 1;
  if(ledVal > 254) ledAdder = -1;
  else if(ledVal < 1) ledAdder = 1;
  ledVal += ledAdder;
  analogWrite(LED, ledVal);
}

void errorLed(){
  static uint8_t ledVal = 0;
  if(ledVal == 0) ledVal = 255;
  else if(ledVal == 255) ledVal = 0;
  analogWrite(LED, ledVal);
}

void finishedLed(){
  static uint8_t stateCounter = 0;
  static uint8_t ledVal = 0;

  if(stateCounter == 7){
    stateCounter = 0;
    ledTimer4.update(finishedDutyMicros);
  }
  if(stateCounter%2) ledVal = 255;
  else ledVal = 0;
  analogWrite(LED, ledVal);
  stateCounter++;
  if(stateCounter == 7)
    ledTimer4.update(1000000);
}

void cidDmp()
{
  cid_t cid;
  if (!sd.readCID(&cid))
  {
    cout << "CID Error" << endl;
  }
  cout << F("\nManufacturer ID: ");
  cout << hex << int(cid.mid) << dec << endl;
  cout << F("OEM ID: ") << cid.oid[0] << cid.oid[1] << endl;
  cout << F("Product: ");
  for (uint8_t i = 0; i < 5; i++)
  {
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

//------------------------------------------------------------------------------
void setup()
{
  pinMode(LED, OUTPUT);
  analogWrite(LED, 255);

  Serial.begin(115200);
  //while (!Serial);

  // use uppercase in hex and use 0X base prefix
  cout << uppercase << showbase << endl;

  trng_init();
  uint32_t tword = trng_word();
  //cout << "random seed: " << tword << endl;
  srandom(tword);
  delay(1000);
}
//------------------------------------------------------------------------------
void loop()
{
  uint32_t t = 0;
  int durationMinutes = 0;
  bool writable = false;
  bool randWriteCompleted = false;
  bool formatCompleted = false;

  ledTimer1.begin(fadeLed, 2000);
  while (!sd.begin(SD_CONFIG)) cout << "no SD" << endl;
  ledTimer1.end();
  analogWrite(LED, 0);

  cout << F("Card size: ") << sd.sectorCount() * 512E-9;
  cout << F(" GB (GB = 1E9 bytes)") << endl;
  cout << F("Speed: ") << sd.kHzSdClk() << F(" kHz") << endl;
  cout << F("sectorCount: ") << sd.sectorCount() << endl;
  cidDmp();

  //actual wiping
  int failedSectorCount = 0;
  std::vector<unsigned int> failedSectors;
#if 1

  cout << F("Starting full disk wipe") << endl
       << endl;
  t = millis();
  uint32_t lastT = t;
  int consecFailCounter = 0;
  int curLedVal = 1;
  int ledAdder = 1;
  progress = 0.0f;

  //start LED progress timer
  ledTimer2.begin(blinkLed, blinkDutyMicros);
  for (unsigned int i = 0; i < sd.sectorCount(); i++)
  {
    if (i % 10 == 0)
    {
      for (size_t i = 0; i < BUF_SIZE / 4; i++)
      {
        buf32[i] = random();
      }
    }
    auto startus = micros();
    bool writeStatus = sd.writeSector(i, buf);

    if (!writeStatus)
    { //error handling
      cout << (int)sd.errorCode() << " ";
      printSdErrorSymbol(&Serial, sd.errorCode());
      cout << " ";
      printSdErrorText(&Serial, sd.errorCode());
      cout << endl;
      failedSectorCount++;
      failedSectors.push_back(i);
    }
    if (micros() - startus > 888888)
    { //hacky fix for random SD Card errors
      cout << "restart sd " << i << endl;
      sd.begin(SD_CONFIG);
      consecFailCounter++;
    }
    else
    {
      consecFailCounter = 0;
    }

    if (consecFailCounter > 1)
    {
      cout << "No SD";
      break;
    }

    if ((millis() - lastT) > 500)
    {
      lastT = millis();
      uint32_t usedtime = millis() - t;
      double msPerSector = (double)usedtime / (double)i;
      uint32_t sectorsLeft = sd.sectorCount() - i;
      cout << ((double)i / (double)sd.sectorCount()) * 100 << " % - " << i * 512E-3 / usedtime << " MB/s - " << usedtime << " " << msPerSector << " " << sectorsLeft << " " << (sectorsLeft * msPerSector) / 60000 << " min left " << curLedVal << endl;
      noInterrupts();
      progress = ((double)i / (double)sd.sectorCount()) * 100;
      interrupts();
    }
    if (i == sd.sectorCount() - 1)
      randWriteCompleted = true;
  }
#endif

//retry failed Sectors
#if 1
  //for(int x = 0; x < 8000; x++)failedSectors.push_back(x);
  if (consecFailCounter == 0)
  {
    cout << "retry " << failedSectors.size() << " failed sectors";
    lastT = t;
    uint32_t curSector = 0;
    for (auto sec : failedSectors)
    {
      int secRetries = 5;
      curSector++;
      for (size_t i = 0; i < BUF_SIZE; i += 4)
      {
        buf[i] = random();
      }
      while (secRetries-- > 0)
      {
        auto startus = micros();
        if (sd.writeSector(sec, buf))
        {
          secRetries = 0;
          failedSectorCount--;
        }
        if (micros() - startus > 888888)
        { //hacky fix for random SD Card errors
          cout << "restart sd " << sec << endl;
          sd.begin(SD_CONFIG);
        }
      }

      if ((millis() - lastT) > 200)
      {
        lastT = millis();
        uint32_t usedtime = millis() - t;
        double msPerSector = (double)usedtime / (double)curSector;
        uint32_t sectorsLeft = failedSectors.size() - curSector;
      }
    }
    cout << "finished, " << failedSectorCount << " sectors couldn't be deleted" << endl;
  }
#endif

//FORMAT SD Card after random data write
// SdCardFactory constructs and initializes the appropriate card.
#ifdef FORMATCARD
  if (consecFailCounter == 0)
  {
    cout << "try formating card now " << endl;
    int formatTries = 10;
    while (formatTries-- > 0 && !formatCompleted)
    {
      cout << "Try Format " << 10 - formatTries << endl;
      formatCompleted = true;
      SdCardFactory cardFactory;
      SdCard *m_card = cardFactory.newCard(SD_CONFIG);
      if (!m_card || m_card->errorCode())
      {
        cout << "card init failed" << endl;
        formatCompleted = false;
      }
      uint8_t sectorBuffer[512];
      FatFormatter fatFormatter;
      if (!fatFormatter.format(m_card, sectorBuffer, &Serial))
      {
        cout << "card format failed" << endl;
        formatCompleted = false;
      }
    }
  }
#endif

  ledTimer2.end();

  t = millis() - t;
  durationMinutes = (int)round((t / 60000));
  cout << endl
       << F("Done") << endl;

  float successRate = ((float)(sd.sectorCount() - failedSectorCount) / (float)sd.sectorCount()) * 100.0f;

  //wait for card disconnect
/*   int t2;
  do
  {
    t2 = millis();
    sd.isBusy();
  } while (millis() - t2 < 900);
  cout << "SD Removed " << endl; */


  

  if(randWriteCompleted){
    //wait for SD removal
    ledTimer4.begin(finishedLed, finishedDutyMicros);
    bool sdStillInserted = true;
    while (sdStillInserted)
    {
      int t2 = millis();
      sd.isBusy();
      int res = millis() - t2;
      if(res > 100)
        sdStillInserted = false;
      delay(100);
    }
    ledTimer4.end();
  }else{
    ledTimer4.begin(errorLed, errorDutyMicros);
    delay(5000);
    ledTimer4.end();
  }

  analogWrite(LED, 0);
  delay(500);
}
