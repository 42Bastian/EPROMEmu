#include <stdint.h>
#include <stddef.h>

enum eEPROMMode : uint8_t
{
    // 24 pin EPROMs
    m2708  = 1,
    m2716  = 2,
    m2732  = 3,

    // 28 pin EPROMs
    m2764  = 4,
    m27128 = 5,
    m27256 = 6,
    m27512 = 7,

    // 32 pin EPROMs
    m27010 = 8,
    m27020 = 9,
    m27040 = 10,
    m27080 = 11, // not enough RAM for this
};

enum eStorageType : uint8_t
{
    SDStorage = 1,
    Flash = 2
};

#define SETTINGS_MAGIC (0xdeadbeef)
struct Settings
{
    uint32_t   magic;
    eEPROMMode epromMode = eEPROMMode::m27040;
    bool       lynxMode = false;
};

extern "C" uint32_t set_arm_clock(uint32_t frequency);

int main();
void setup();
void readChunk(uint8_t* buffer, size_t bufferSize, const char* filename);
void readIncomingROM();
void setPinMode(const int32_t* pins, uint32_t direction);
void setStorageType();
void getSettings();
void saveSettings();

//  DM     Non-DMA
// GPIO1 == GPIO6
// GPIO2 == GPIO7
// GPIO3 == GPIO8
// GPIO4 == GPIO9

// addr
// GPIO6-16 -> 19 A0
// GPIO6-17 -> 18 A1
// GPIO6-18 -> 14 A2
// GPIO6-19 -> 15 A3
// GPIO6-20 -> 40 A4
// GPIO6-21 -> 41 A5
// GPIO6-22 -> 17 A6
// GPIO6-23 -> 16 A7
// GPIO6-24 -> 22 A8
// GPIO6-25 -> 23 A9
// GPIO6-26 -> 20 A10
// GPIO6-27 -> 21 A11
// GPIO6-28 -> 38 A12
// GPIO6-29 -> 39 A13
// GPIO6-30 -> 26 A14
// GPIO6-31 -> 27 A15
// --
// GPIO9-04 -> 02 A16
// GPIO9-05 -> 03 A17
// GPIO9-06 -> 04 A18
// GPIO9-07 -> 33 A19

// data
// GPIO7-00 -> 10 D0
// GPIO7-01 -> 12 D1
// GPIO7-02 -> 11 D2
// GPIO7-03 -> 13 D3
// --
// GPIO7-16 -> 08 D4
// GPIO7-17 -> 07 D5
// GPIO7-18 -> 36 D6
// GPIO7-19 -> 37 D7

// ---------------------------

// the rest
// GPIO6-02 -> 01
// GPIO6-03 -> 00
// GPIO6-12 -> 24
// GPIO6-13 -> 25

// GPIO7-10 -> 06
// GPIO7-11 -> 09
// GPIO7-12 -> 32
// GPIO7-28 -> 35
// GPIO7-29 -> 34

// GPIO8-12 -> 45
// GPIO8-13 -> 44
// GPIO8-14 -> 43
// GPIO8-15 -> 42
// GPIO8-16 -> 47
// GPIO8-17 -> 46
// GPIO8-18 -> 28
// GPIO8-22 -> 31
// GPIO8-23 -> 30

// GPIO9-08 -> 05
// GPIO9-22 -> 51
// GPIO9-24 -> 48
// GPIO9-25 -> 53
// GPIO9-26 -> 52
// GPIO9-27 -> 49
// GPIO9-28 -> 50
// GPIO9-29 -> 54
// GPIO9-31 -> 29