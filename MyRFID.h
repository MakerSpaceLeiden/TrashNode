#ifndef _H_MYRFID
#define _H_MYRFID

#include <stddef.h>
#include <functional>

#include <ACNode-private.h>
#include <ACBase.h>
#include <MFRC522.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <Wire.h>

// I2C based NFC reader

class MyRFID : public ACBase {
  public:
    const char * name() { return "RFID"; }
   
    MyRFID(TwoWire *i2cBus, bool useCache = true);

    void begin();

    bool CheckPN53xBoardAvailable();

    void loop();

    void report(JsonObject& report);

    typedef std::function<ACBase::cmd_result_t(const char *)> THandlerFunction_SwipeCB;

    MyRFID& onSwipe(THandlerFunction_SwipeCB fn) { _swipe_cb = fn; return *this; };
  
  private:
    bool foundPN53xBoard = false;
    bool useTagsStoredInCache = false;

    PN532_I2C * _i2cNFCDevice;
    PN532 * _nfc532;
    
    THandlerFunction_SwipeCB _swipe_cb = NULL;

    char lasttag[MAX_TAG_LEN * 4];      // Up to a 3 digit byte and a dash or terminating \0. */
    unsigned long lastswipe, _scan, _miss;
    unsigned long nextCheck = 0;
    bool tagDecoded = false;
};
#endif
