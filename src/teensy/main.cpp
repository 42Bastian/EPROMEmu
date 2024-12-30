/*
** Teensy 4.1 firmware for LynxCartEmu
**
** (c) 2024 Brian Peek (original) and 42Bastian Schick (Lynx version)
**
*/
// FLASHMEM
// DMAMEM
// PROGMEM

#include <Arduino.h>
#include <SD.h>
#include <stdint.h>
#include "ubll.h"

enum cardSize : uint8_t {
  _128K = 1,
  _256K = 2,
  _512K = 3,
  _1M = 4,
};

typedef struct
{
  uint8_t  magic[4];
  uint16_t page_size_bank0;
  uint16_t page_size_bank1;
  uint16_t version;
  char     cartname[32];
  char     manufname[16];
  char     rotation;
  char     audBit;
  char     eeprom;
  char     spare[3];
} lnx_header_t;

constexpr size_t   ROM_BUFFER_LEN       = (256*1024);
constexpr uint32_t ROM_BUFFER_HI_MASK   = (ROM_BUFFER_LEN-1);
constexpr size_t   TOTAL_ROM_BUFFER_LEN = (2*ROM_BUFFER_LEN);

uint8_t bufferLo[ROM_BUFFER_LEN] {};
DMAMEM uint8_t bufferHi[ROM_BUFFER_LEN] {};

const int32_t inPins[]  = {
  19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,2,3,4,33, -1
};
const int32_t outPins[] = {
  10,12,11,13,8,7,36,37, -1
};

constexpr const char *filenameLNX      = "game.lnx";
FS* g_fs = nullptr;
lnx_header_t lnx_header;
cardSize cSize = _256K;

int readLNX(Stream *s)
{
  size_t read;
  size_t total;
  uint8_t *buffer;
  size_t expected;

  memset(bufferLo, 0x42, sizeof(bufferLo));
  memset(bufferHi, 0x42, sizeof(bufferHi));
  memset(&lnx_header, 0xea, sizeof(lnx_header_t));

  read = s->readBytes((uint8_t *)&lnx_header, sizeof(lnx_header_t));

  expected = lnx_header.page_size_bank0 * 256;
  expected += lnx_header.page_size_bank1 * 256;

  total = 0;
  buffer = bufferLo;
  while( total < sizeof(bufferLo) && expected > 0 ){
    size_t wanted;

    wanted = sizeof(bufferLo) - total;
    wanted = wanted > 1024 ? 1024 : wanted;
    read = s->readBytes(buffer, wanted);
    if ( read <= 0 ) return 0;
    total += read;
    buffer += read;
    expected -= read;
  }
  total -= sizeof(bufferLo);

  buffer = bufferHi;
  while( total < sizeof(bufferHi) && expected > 0 ){
    size_t wanted;

    wanted = sizeof(bufferHi) - total;
    wanted = wanted > 1024 ? 1024 : wanted;
    read = s->readBytes(buffer, wanted);
    if ( read <= 0 ) return 0;
    total += read;
    buffer += read;
    expected -= read;
  }

  if( expected != 0 ) {
    return 0;
  }

  switch (lnx_header.page_size_bank0){
  case 512:
    cSize = _128K;
    break;
  case 1024:
    cSize = _256K;
    break;
  case 2048:
    cSize = _512K;
    break;
  default:
    cSize = _1M;
  }

  if ( g_fs->exists(filenameLNX)) {
    g_fs->remove(filenameLNX);
  }

  File f = g_fs->open(filenameLNX, FILE_WRITE);

  if ( f ){
    f.write((uint8_t *)&lnx_header, sizeof(lnx_header_t));

    expected = lnx_header.page_size_bank0 * 256;
    expected += lnx_header.page_size_bank1 * 256;

    f.write(bufferLo, min(sizeof(bufferLo),expected));
    f.write(bufferHi, expected-sizeof(bufferLo));
    f.close();
  }
  return 1;
}

void setPinMode(const int32_t* pins, uint32_t direction)
{
  for(int i = 0; pins[i] != -1; i++){
    pinMode(pins[i], direction);
  }
}

void checkSD()
{
  SD.begin(BUILTIN_SDCARD);
  g_fs = (FS*)&SD;

  if ( g_fs->exists(filenameLNX)) {
    Serial.printf("=> Found game.lnx:");

    File f = g_fs->open(filenameLNX, FILE_READ);
    int32_t sz = (int32_t)f.size();

    f.read(&lnx_header,64);
    Serial.printf("Size %ld\r\n",sz);
    Serial.flush();
    sz -= 64;
    f.read(bufferLo, min(sz,(int32_t)sizeof(bufferLo)));
    sz -= sizeof(bufferLo);
    if ( sz > 0 ){
      f.read(bufferHi,sz);
    }
    switch (lnx_header.page_size_bank0){
    case 512:
      cSize = _128K;
      break;
    case 1024:
      cSize = _256K;
      break;
    case 2048:
      cSize = _512K;
      break;
    default:
      cSize = _1M;
    }
    Serial.printf("Banksize: %d\r\n%s\r\n",cSize,lnx_header.cartname);
    f.close();
  } else {
    Serial.printf("No file found, switch to uBLL\r\n");
    memcpy(bufferLo, uBLL, sizeof(uBLL));
    strcpy(lnx_header.cartname, "uBLL");
    lnx_header.page_size_bank0 = 1024;
    lnx_header.page_size_bank1 = 0;
    cSize = _256K;
  }
}

void setup()
{
  Serial.begin(115200*8);

  checkSD();

  setPinMode(inPins, INPUT);
  setPinMode(outPins, OUTPUT);
  GPIO7_DR = 0;
}

void loop()
{
  uint32_t io6 = (GPIO6_DR >> 16);      //  A0 - A15
  uint32_t io9 = GPIO9_DR;              // A16 - A19
  uint32_t addr = 0;                    // calculated address

  switch(cSize) {
    // 1Mbit, 128KB, 0x00000-0x1FFFF
  case _128K:
    addr =    io6 & 0x001FF;         // Lynx A0  - A8  -> EPROM A0 - A8
    addr |=  (io6 >> 2)  & 0x03E00;  // Lynx A12 - A17 -> EPROM A11 - A15
    addr |= ((io9 << 10) & 0x1C000); // Lynx A18 - A19 -> EPROM A16
    break;

    // 2Mbit, 256KB, 0x00000-0x3FFFF
  case _256K:
    addr =    io6 & 0x003FF;         // Lynx A0  - A9  -> EPROM A0 - A9
    addr |=  (io6 >> 1)  & 0x07C00;  // Lynx A12 - A17 -> EPROM A11 - A15
    addr |= ((io9 << 11) & 0x38000); // Lynx A18 - A19 -> EPROM A16 - A17
    break;

    // 4Mbit, 512KB, 0x00000-0x7FFFF
  case _512K:
    // XXX: check for AUDIN
    addr =   io6;  // Lynx A0  - A10, A12-A17 -> EPROM A0 - A10, A11-A15
    addr |= ((io9 << 12) & 0x70000); // Lynx A18 - A19 -> EPROM A16 - A17
    break;

    // 8Mbit, 1MB, 0x00000-0xFFFFF
  case _1M:
    // XXX: check for AUDIN
    addr =   io6;  // Lynx A0  - A10, A12-A17 -> EPROM A0 - A10, A11-A15
    addr |= ((io9 << 12) & 0x70000); // Lynx A18 - A19 -> EPROM A16 - A17
    break;
  }

  uint8_t b;

  // get byte at addr
  if(addr < ROM_BUFFER_LEN){
    b = bufferLo[addr];
  } else {
    b = bufferHi[addr & ROM_BUFFER_HI_MASK];
  }

  // set data pins
  GPIO7_DR = (b & 0x0F) | ((b & 0xF0) << 12);

  // read file from serial port, if available
  if(Serial.available()){
    char command[2];
    command[0] = 0;
    Serial.readBytes(command,2);
    if ( command[0] == 'c' ){
      switch (command[1] ){
      case 'r':
        checkSD();
        break;
      case 'l':
        if ( !readLNX(&Serial) ){
          checkSD();
        }
        break;
      case 'i':
        Serial.printf("Name: %s\r\n"
                      "Page0: %d\r\n"
                      "Page1: %d\r\n"
                      ,lnx_header.cartname
                      ,lnx_header.page_size_bank0
                      ,lnx_header.page_size_bank1
                      );
        break;

      case 'h':
        {
//->          uint8_t *p = (uint8_t *)bufferLo;
//->          for(int i = 0; i < 64;++i){
//->            Serial.printf("%02x ",*p++);
//->          }
//->          Serial.printf("\n\r");
        }
        break;
      }
    }
  }
}
