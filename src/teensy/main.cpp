#include <Arduino.h>
#include <SD.h>
#include <LittleFS.h>
#include <EEPROM.h>
#include "main.h"

constexpr const char *fnLo       = "romLo.bin";
constexpr const char *fnHi       = "romHi.bin";

constexpr size_t   ROM_BUFFER_LEN       = (256*1024);
constexpr uint32_t ROM_BUFFER_HI_MASK   = (ROM_BUFFER_LEN-1);
constexpr size_t   TOTAL_ROM_BUFFER_LEN = (2*ROM_BUFFER_LEN);
constexpr size_t   READ_CHUNK_LENGTH    = (1024);

       uint8_t bufferLo[ROM_BUFFER_LEN] __attribute__ ((aligned(32))) {};
DMAMEM uint8_t bufferHi[ROM_BUFFER_LEN] __attribute__ ((aligned(32))) {};

const int32_t inPins[]  = { 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,2,3,4,33, -1 };
const int32_t outPins[] = { 10,12,11,13,8,7,36,37, -1 };

LittleFS_Program lfs;
Settings g_settings {};
FS* g_fs = nullptr;
eStorageType g_storageType;

int main()
{
    //set_arm_clock(816000000);

    setup();

    while(1)
    {
        uint32_t io6 = (GPIO6_DR >> 16);	//  A0 - A15
        uint32_t io9 = GPIO9_DR;			// A16 - A19
        uint32_t addr = 0;	                // calculated address
        uint8_t b;		                    // byte at address

        switch(g_settings.epromMode)
        {
            // 8Kbit, 1KB, 0x0000-0x03FF
            case m2708:
                addr = (io6 & 0x07FF);
                break;

            // 16Kbit, 2KB, 0x0000-0x07FF
            case m2716:
                addr = (io6 & 0x07FF);
                break;

            // 32Kbit, 4KB, 0x0000-0x0FFF
            case m2732:
                addr = (io6 & 0x0FFF);
                break;

            // 64Kbit, 8KB, 0x0000-0x1FFF
            case m2764:
                addr = (io6 & 0x1FFF);
                break;

            // 128Kbit, 16KB, 0x0000-0x3FFF
            case m27128:
                addr = (io6 & 0x3FFF);
                break;

            // 256Kbit, 32KB, 0x0000-0x7FFF
            case m27256:
                addr = (io6 & 0x7FFF);
                break;

            // 512Kbit, 64KB, 0x0000-0xFFFF
            case m27512:
                addr = (io6 & 0xFFFF);
                break;

            // 1Mbit, 128KB, 0x00000-0x1FFFF
            case m27010:
                if(g_settings.lynxMode)
                {
                    addr =   io6 & 0x001FF;        // Lynx A0 - A8   -> EPROM A0 - A8
                    addr |= (io6 >> 2)  & 0x03E00; // Lynx A12 - A17 -> EPROM A11 - A15
                    addr |= (io9 << 10) & 0x1C000; // Lynx A18 - A19 -> EPROM A16
                }
                else
                    addr = (io6 & 0xFFFF) | ((io9 << 12) & 0x10000);
                break;

            // 2Mbit, 256KB, 0x00000-0x3FFFF
            case m27020:
                if(g_settings.lynxMode)
                {
                    addr =   io6 & 0x003FF;        // Lynx A0 - A9   -> EPROM A0 - A9
                    addr |= (io6 >> 1)  & 0x07C00; // Lynx A12 - A17 -> EPROM A11 - A15
                    addr |= (io9 << 11) & 0x38000; // Lynx A18 - A19 -> EPROM A16 - A17
                }
                else
                    addr = (io6 & 0xFFFF) | ((io9 << 12) & 0x30000);
                break;

            // 4Mbit, 512KB, 0x00000-0x7FFFF
            case m27040:
                addr = (io6 & 0xFFFF) | ((io9 << 12) & 0x70000);
                break;

            // 8Mbit, 1MB, 0x00000-0xFFFFF
            case m27080:
                addr = (io6 & 0xFFFF) | ((io9 << 12) & 0xF0000);
                break;
        }

        // get byte at addr from Lo or Hi buffer
        b = (addr < ROM_BUFFER_LEN) ? bufferLo[addr] : bufferHi[addr & ROM_BUFFER_HI_MASK];

        // set data pins
        // it's really (b & 0x0F) | ((b & 0xF0) << 12)
        // but we don't care about the middle GPIO7 pins that will be written
        GPIO7_DR = b | (b << 12);

        // read next ROM from serial port, if available
        readIncomingROM();
    }

    return 0;
}

void setup()
{
    // init SD and internal flash
    SD.begin(BUILTIN_SDCARD);
    lfs.begin(TOTAL_ROM_BUFFER_LEN*2);

    setStorageType();

    getSettings();

    // read lower chunk
    if(g_fs->exists(fnLo))
    {
        File f = g_fs->open(fnLo, FILE_READ);
        f.read(bufferLo, f.size());
        f.close();
    }

    // read upper chunk, if it exists
    if(g_fs->exists(fnHi))
    {
        File f = g_fs->open(fnHi, FILE_READ);
        f.read(bufferHi, f.size());
        f.close();
    }

    // setup our pins for input/output
    setPinMode(inPins, INPUT);
    setPinMode(outPins, OUTPUT);

    // set default D0-D7
    GPIO7_DR = 0xFF;
}

void readIncomingROM()
{
    if(!Serial.available())
        return;

    // determine where we'll save this data
    setStorageType();

    // pull the settings
    g_settings.epromMode = (eEPROMMode)Serial.read();
    g_settings.lynxMode  = (bool)Serial.read();

    // let the client know where the ROM will be saved
    Serial.write(g_storageType);
    Serial.flush();

    // write settings
    saveSettings();

    // read/write lower chunk
    readChunk(bufferLo, ROM_BUFFER_LEN, fnLo);

    // read/write upper chunk
    readChunk(bufferHi, ROM_BUFFER_LEN, fnHi);
}

void readChunk(uint8_t* buffer, size_t bufferSize, const char* filename)
{
    char chunk[READ_CHUNK_LENGTH];
    size_t read = 0;
    size_t total = 0;
    uint8_t* p = buffer;

    // clear out old data
    memset(buffer, 0xFF, bufferSize);

    // read to the end of the buffer, or the end of the data
    do
    {
        memset(chunk, 0xFF, READ_CHUNK_LENGTH);

        read = Serial.readBytes(chunk, READ_CHUNK_LENGTH);

        if(read > 0)
        {
            memcpy(p, chunk, read);
            p += read;
            total += read;
        }
    }
    while(read != 0 && total < bufferSize);

    // remove the existing file and save the new data
    g_fs->remove(filename);

    if(total > 0)
    {
        File f = g_fs->open(filename, FILE_WRITE);
        f.write(buffer, total);
        f.close();
    }
}

void setPinMode(const int32_t* pins, uint32_t direction)
{
    for(int i = 0; pins[i] != -1; i++)
        pinMode(pins[i], direction);
}

void setStorageType()
{
    if(SD.mediaPresent())
    {
        g_storageType = eStorageType::SDStorage;
        g_fs = (FS*)&SD;
    }
    else
    {
        g_storageType = eStorageType::Flash;
        g_fs = (FS*)&lfs;
    }
}

void getSettings()
{
    EEPROM.get(0, g_settings);

    // if our magic is wrong, reset to default
    if(g_settings.magic != SETTINGS_MAGIC)
    {
        g_settings.epromMode = eEPROMMode::m27040;
        g_settings.lynxMode = false;
    }
}

void saveSettings()
{
    g_settings.magic = SETTINGS_MAGIC;
    EEPROM.put(0, g_settings);
}
