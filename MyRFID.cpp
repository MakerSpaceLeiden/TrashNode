#include "MyRFID.h"
// https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf

volatile bool myCardScannedIrqSeen = false;
static void readCard() { myCardScannedIrqSeen = true; }

MyRFID::MyRFID(const byte sspin , const byte rstpin , const byte irqpin , const byte spiclk , const byte spimiso , const byte spimosi ) 
{
#ifdef ESP32
  if (spiclk != 255 || spimiso != 255 || spimosi != 255)
     SPI.begin(spiclk, spimiso, spimosi);
  else
     SPI.begin();
#endif

nfcCardUsed = false;

/*
/Users/dirkx/Documents/Arduino/libraries/ACNode/src/RFID.cpp: In constructor 'RFID::RFID(byte, byte, byte, byte, byte, byte)':
/Users/dirkx/Documents/Arduino/libraries/ACNode/src/RFID.cpp:15:27: error: invalid conversion from 'MFRC522_SPI*' to 'byte {aka unsigned char}' [-fpermissive]
    _mfrc522 = new MFRC522(_spiDevice);
                           ^
In file included from /Users/dirkx/Documents/Arduino/libraries/ACNode/src/RFID.h:9:0,
                 from /Users/dirkx/Documents/Arduino/libraries/ACNode/src/RFID.cpp:1:
/Users/dirkx/Documents/Arduino/libraries/rfid-org/src/MFRC522.h:347:2: note:   initializing argument 1 of 'MFRC522::MFRC522(byte)'
  MFRC522(byte resetPowerDownPin);
  ^
*/
   _spiDevice = new MFRC522_SPI(sspin, rstpin, &SPI);
   _mfrc522 = new MFRC522(_spiDevice);

  if (irqpin != 255)  {
  	pinMode(irqpin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(irqpin), readCard, FALLING);
	_irqMode = true;
   };
}

MyRFID::MyRFID(TwoWire *i2cBus, const byte i2caddr, const byte rstpin, const byte irqpin) 
{
   _i2cDevice = new MFRC522_I2C(rstpin, i2caddr, *i2cBus);
   _mfrc522 = new MFRC522(_i2cDevice);

  if (irqpin != 255)  {
  	pinMode(irqpin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(irqpin), readCard, FALLING);
	_irqMode = true;
   };
}

MyRFID::MyRFID(bool useCache, bool useNFCRFIDCard) {
   nfcCardUsed = useNFCRFIDCard;
   useTagsStoredInCache = useCache;

   if (nfcCardUsed) {
      Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN, RFID_I2C_FREQ);
      _i2cNFCDevice = new PN532_I2C(Wire);
      _nfc532 = new PN532(*_i2cNFCDevice);
      return;
   }
   _mfrc522->PCD_Init();     // Init MFRC522

   if (true == _irqMode) {
      _mfrc522->PCD_WriteRegister(_mfrc522->ComIEnReg, 0xA0 /* irq on read */);
      myCardScannedIrqSeen = false; 
      Serial.println("MFRC522: IRQ mode.");
   } else {
      Serial.println("MFRC522: Polling mode.");
   };

   // Note: this seems to wedge certain cards.
   if (_debug) {
      _mfrc522->PCD_DumpVersionToSerial();
   }
}

bool MyRFID::CheckPN53xBoardAvailable()
{
   uint32_t versiondata = _nfc532->getFirmwareVersion();
   if (! versiondata) {
      if (foundPN53xBoard) {
         Serial.println("RFId: Didn't find PN53x board");
         foundPN53xBoard = false;
      }
   } else {
      if (!foundPN53xBoard) {
         Serial.println("RFId: Found PN53x board");
         foundPN53xBoard = true;
      }
   }
   return foundPN53xBoard;
}

void MyRFID::begin() {
   if (nfcCardUsed) {
      _nfc532->begin();
      uint32_t versiondata = _nfc532->getFirmwareVersion();
      if (! versiondata) {
         Serial.println("RFId: Didn't find PN53x board");
         foundPN53xBoard = false;
      } else {
         foundPN53xBoard = true;
         Serial.println("RFId: Found PN53x board");
         // configure board to read RFID tags
         _nfc532->SAMConfig();
      }      
   } else {
      _mfrc522->PCD_Init();     // Init MFRC522

      if (true == _irqMode) {
         _mfrc522->PCD_WriteRegister(_mfrc522->ComIEnReg, 0xA0 /* irq on read */);
         myCardScannedIrqSeen = false; 
         Serial.println("MFRC522: IRQ mode.");
      } else {
         Serial.println("MFRC522: Polling mode.");
      };

      // Note: this seems to wedge certain cards.
      if (_debug) {
         _mfrc522->PCD_DumpVersionToSerial();
      }
   }
}

void MyRFID::loop() {
   if (nfcCardUsed) {
      if (foundPN53xBoard) {
         uint8_t success;
         uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
         uint8_t uidLength;                                       // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
                                                                  // maximun 12 bytes for other types
         // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
         // 'uid' will be populated with the UID, and uidLength will indicate
         // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)

         if ((millis() > nextCheck)) {
            success = _nfc532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 20);
            if (success && uidLength && !tagDecoded) {
               tagDecoded = true;
               char tag[MAX_TAG_LEN * 4] = { 0 };
               for (int i = 0; i < uidLength; i++) {
                  char buff[5];
                  snprintf(buff, sizeof(buff), "%s%d", i ? "-" : "", uid[i]);
                  strncat(tag, buff, sizeof(tag));
               };
               // Log.printf("Tag ID = %s\n", tag);
               Serial.printf("Tag ID = %s\n\r", tag);

               // Limit the rate of reporting. Unless it is a new tag.
               //
               if (strncmp(lasttag, tag, sizeof(lasttag)) || millis() - lastswipe > 3000) {
                     lastswipe = millis();
                  strncpy(lasttag, tag, sizeof(tag));

                  if (!_swipe_cb || (_swipe_cb(lasttag) != ACNode::CMD_CLAIMED)) {
                        // Simple approval request; default is to 'energise' the contactor on 'machine'.
                     Log.println("Requesting approval");
                    // _acnode->request_approval_devices(lasttag, NULL,NULL, useTagsStoredInCache);
                  } else {
                     Debug.println( _swipe_cb ? "internal rq used " : "callback claimed" );
                  };
               };
               _scan++;
            } else {
               if (!success) {
                  tagDecoded = false;
               }
               if (success && (uidLength <= 0)) {
                  _miss++;
               }
            }
            nextCheck = millis() + 100;
         }
      }      
      return;      
   } else {
      // if we are in IRQ mode; and we've seen no card; then just
      // periodically re-arm the reader. Every few seconds seems to
      // be enough. XX find datasheet and put in corect time.
      //
      if (true == _irqMode) {
         if (false == myCardScannedIrqSeen) {
            static unsigned long kick = 0;
            if (millis() - kick > 500) {
               kick = millis();
               _mfrc522->PCD_WriteRegister(_mfrc522->FIFODataReg, _mfrc522->PICC_CMD_REQA);
               _mfrc522->PCD_WriteRegister(_mfrc522->CommandReg, _mfrc522->PCD_Transceive);
               _mfrc522->PCD_WriteRegister(_mfrc522->BitFramingReg, 0x87); // start data transmission, last byte bits, 9.3.1.14
            };
            return;
         };
      } else {
      // Polling mode does not require a re-arm; instead we check fi there is a card 'now;
         if (_mfrc522->PICC_IsNewCardPresent() == 0)
            return;
      }
      if (_mfrc522->PICC_ReadCardSerial() &&  _mfrc522->uid.size) {
         char tag[MAX_TAG_LEN * 4] = { 0 };
         for (int i = 0; i < _mfrc522->uid.size; i++) {
            char buff[5];
            snprintf(buff, sizeof(buff), "%s%d", i ? "-" : "", _mfrc522->uid.uidByte[i]);
            strncat(tag, buff, sizeof(tag));
         };
         // Log.printf("Tag ID = %s\n", tag);
         Serial.printf("Tag ID = %s\n\r", tag);

         // Limit the rate of reporting. Unless it is a new tag.
         //
         if (strncmp(lasttag, tag, sizeof(lasttag)) || millis() - lastswipe > 3000) {
            lastswipe = millis();
            strncpy(lasttag, tag, sizeof(tag));

            if (!_swipe_cb || (_swipe_cb(lasttag) != ACNode::CMD_CLAIMED)) {
                  // Simple approval request; default is to 'energise' the contactor on 'machine'.
               Log.println("Requesting approval");
               // _acnode->request_approval_devices(lasttag);
            } else {
               Debug.println( _swipe_cb ? "internal rq used " : "callback claimed" );
            };
         };
         _scan++;
      } else {
         _miss++;
      };

      // Stop the reading.
      _mfrc522->PICC_HaltA();

      // clear the interupt and re-arm the reader.
      if (_irqMode) {
      _mfrc522->PCD_WriteRegister(_mfrc522->ComIrqReg, 0x7F);
         myCardScannedIrqSeen = false;
      };

      return;
   }
}

void MyRFID::report(JsonObject& report) {
	report["rfid_scans"] = _scan;
	report["rfid_misses"] = _miss;
}
