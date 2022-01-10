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

// SPI based RFID reader
// if POESP board (board Aart) is used
#ifndef RFID_MOSI_PIN
#define RFID_MOSI_PIN   (13)
#endif

#ifndef RFID_MISO_PIN
#define RFID_MISO_PIN   (12)
#endif

#ifndef RFID_CLK_PIN
#define RFID_CLK_PIN    (14)
#endif

#ifndef RFID_SELECT_PIN
#define RFID_SELECT_PIN (15)
#endif

#ifndef RFID_RESET_PIN
#define RFID_RESET_PIN  (32)
#endif

#ifndef RFID_IRQ_PIN
#define RFID_IRQ_PIN    (33) // Set to -1 to switch to polling mode; 33 to use IRQs
#endif

// I2C based NFC reader
// if ESP32-PoE is used
#ifndef RFID_SDA_PIN
#define RFID_SDA_PIN    (13)
#endif

#ifndef RFID_SCL_PIN
#define RFID_SCL_PIN    (16)
#endif

#ifndef RFID_I2C_FREQ
#define RFID_I2C_FREQ   (100000U)
#endif

class MyRFID : public ACBase {
  public:
    const char * name() { return "RFID"; }
    
    MyRFID(const byte sspin = RFID_SELECT_PIN, const byte rstpin = RFID_RESET_PIN, const byte irqpin = RFID_IRQ_PIN, 
	       const byte spiclk = RFID_CLK_PIN, const byte spimiso = RFID_MISO_PIN, const byte spimosi = RFID_MOSI_PIN);

    MyRFID(TwoWire *i2cBus, const byte i2caddr, const byte rstpin = RFID_RESET_PIN, const byte irqpin = RFID_IRQ_PIN);

    MyRFID(bool useCache = true, bool useNFCRFIDCard = true);

    void begin();

    bool CheckPN53xBoardAvailable();

    void loop();

    void report(JsonObject& report);

    typedef std::function<ACBase::cmd_result_t(const char *)> THandlerFunction_SwipeCB;

    MyRFID& onSwipe(THandlerFunction_SwipeCB fn) 
	{ _swipe_cb = fn; return *this; };
  
  private:
    bool _irqMode = false;
    bool nfcCardUsed = false;
    bool foundPN53xBoard = false;
    bool useTagsStoredInCache = false;

    MFRC522_SPI * _spiDevice;
    MFRC522_I2C * _i2cDevice;
    MFRC522 * _mfrc522;

    PN532_I2C * _i2cNFCDevice;
    PN532 * _nfc532;
    
    THandlerFunction_SwipeCB _swipe_cb = NULL;

    char lasttag[MAX_TAG_LEN * 4];      // Up to a 3 digit byte and a dash or terminating \0. */
    unsigned long lastswipe, _scan, _miss;
    unsigned long nextCheck = 0;
    bool tagDecoded = false;
};
#endif
