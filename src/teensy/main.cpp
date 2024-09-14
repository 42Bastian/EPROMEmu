// FLASHMEM
// DMAMEM
// PROGMEM

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <stdint.h>

enum eMode : uint8_t
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
	m27080 = 11,
};

struct Settings
{
	eMode epromMode = m27020;
	bool  lynxMode = false;
};

constexpr size_t   ROM_BUFFER_LEN       = (256*1024);
constexpr uint32_t ROM_BUFFER_HI_MASK   = (ROM_BUFFER_LEN-1);
constexpr size_t   TOTAL_ROM_BUFFER_LEN = (2*ROM_BUFFER_LEN);
constexpr size_t   READ_CHUNK_LENGTH    = (1024);

       uint8_t bufferLo[ROM_BUFFER_LEN] {};
DMAMEM uint8_t bufferHi[ROM_BUFFER_LEN] {};

const int32_t inPins[]  = { 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,2,3,4,33, -1 };
const int32_t outPins[] = { 10,12,11,13,8,7,36,37, -1 };

constexpr const char *filenameSettings = "settings.bin";
constexpr const char *filenameLo       = "romLo.bin";
constexpr const char *filenameHi       = "romHi.bin";

FS* g_fs = nullptr;
Settings g_settings {};

void readChunk(Stream* s, uint8_t* buffer, size_t size, const char* filename)
{
	uint8_t chunk[READ_CHUNK_LENGTH];
	size_t read = 0;
	size_t total = 0;
	uint8_t* p = buffer;

	memset(buffer, 0xFF, size);

	do
	{
		memset(chunk, 0, READ_CHUNK_LENGTH);

		read = s->readBytes(chunk, READ_CHUNK_LENGTH);

		if(read > 0)
		{
			memcpy(p, chunk, read);
			p += read;
			total += read;
		}
	}
	while(read != 0 && total < size);

	g_fs->remove(filename);

	if(total > 0)
	{
		File f = g_fs->open(filename, FILE_WRITE);
		f.write(buffer, total);
		f.close();
	}
}

void readData(Stream* s)
{
	g_settings.epromMode = (eMode)s->read();
	g_settings.lynxMode  = (bool)s->read();

	g_fs->remove(filenameSettings);
	File f = g_fs->open(filenameSettings, FILE_WRITE);
	f.write(&g_settings, sizeof(Settings));
	f.close();

	readChunk(s, bufferLo, ROM_BUFFER_LEN, filenameLo);
	readChunk(s, bufferHi, ROM_BUFFER_LEN, filenameHi);
}

void setPinMode(const int32_t* pins, uint32_t direction)
{
	for(int i = 0; pins[i] != -1; i++)
		pinMode(pins[i], direction);
}

void setup()
{
	Serial.begin(115200);

	SD.begin(BUILTIN_SDCARD);
	g_fs = (FS*)&SD;

	if(g_fs->exists(filenameSettings))
	{
		File f = g_fs->open(filenameSettings, FILE_READ);
		f.read(&g_settings, sizeof(Settings));
		f.close();
	}

	if(g_fs->exists(filenameLo))
	{
		File f = g_fs->open(filenameLo, FILE_READ);
		f.read(bufferLo, f.size());
		f.close();
	}

	if(g_fs->exists(filenameHi))
	{
		File f = g_fs->open(filenameHi, FILE_READ);
		f.read(bufferHi, f.size());
		f.close();
	}

	setPinMode(inPins, INPUT);
	setPinMode(outPins, OUTPUT);
	GPIO7_DR = 0;
}

void loop()
{
	uint32_t io6 = (GPIO6_DR >> 16);	//  A0 - A15
	uint32_t io9 = GPIO9_DR;			// A16 - A19
	uint32_t addr = 0;					// calculated address

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
				addr =    io6 & 0x001FF;         // Lynx A0 - A8   -> EPROM A0 - A8
				addr |=  (io6 >> 2)  & 0x03E00;  // Lynx A12 - A17 -> EPROM A11 - A15
				addr |= ((io9 << 10) & 0x1C000); // Lynx A18 - A19 -> EPROM A16
			}
			else
				addr = (io6 & 0xFFFF) | ((io9 << 12) & 0x10000);
			break;

		// 2Mbit, 256KB, 0x00000-0x3FFFF
		case m27020:
			if(g_settings.lynxMode)
			{
				addr =    io6 & 0x003FF;         // Lynx A0 - A9   -> EPROM A0 - A9
				addr |=  (io6 >> 1)  & 0x07C00;  // Lynx A12 - A17 -> EPROM A11 - A15
				addr |= ((io9 << 11) & 0x38000); // Lynx A18 - A19 -> EPROM A16 - A17
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

	uint8_t b;

	// get byte at addr
	if(addr < ROM_BUFFER_LEN)
		b = bufferLo[addr];
	else
		b = bufferHi[addr & ROM_BUFFER_HI_MASK];

	// set data pins
	GPIO7_DR = (b & 0x0F) | ((b & 0xF0) << 12);

	// read file from serial port, if available
	if(Serial.available())
		readData(&Serial);
}

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