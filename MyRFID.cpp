#include "MyRFID.h"
// https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf

/*
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
*/

MyRFID::MyRFID(bool useCache) {
  useTagsStoredInCache = useCache;
  Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN, RFID_I2C_FREQ);
  _i2cNFCDevice = new PN532_I2C(Wire);
  _nfc532 = new PN532(*_i2cNFCDevice);
}

bool MyRFID::CheckPN53xBoardAvailable()
{
   uint32_t versiondata = _nfc532->getFirmwareVersion();
   if (! versiondata) {
      if (foundPN53xBoard) {
         Serial.println("RFID: Didn't find PN53x board");
         foundPN53xBoard = false;
      }
   } else {
      if (!foundPN53xBoard) {
         Serial.println("RFID: Found PN53x board");
         foundPN53xBoard = true;
      }
   }
   return foundPN53xBoard;
}

void MyRFID::begin() {
    _nfc532->begin();
    uint32_t versiondata = _nfc532->getFirmwareVersion();
    if (! versiondata) {
       Serial.println("RFID: Didn't find PN53x board");
       foundPN53xBoard = false;
    } else {
       foundPN53xBoard = true;
       Serial.println("RFID: Found PN53x board");
       // configure board to read RFID tags
       _nfc532->SAMConfig();
    }      
}

void MyRFID::loop() {
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
}

void MyRFID::report(JsonObject& report) {
	report["rfid_scans"] = _scan;
	report["rfid_misses"] = _miss;
}
