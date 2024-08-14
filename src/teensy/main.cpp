#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// FLASHMEM
// DMAMEM
// PROGMEM

enum eMode : unsigned char
{
	m27C256 = 1,
	m27C010 = 2,
	m27C020 = 3,
	m27C040 = 4,
	m27C080 = 5,
};

#define ROM_BUFFER_LEN (1024*1024)
EXTMEM char buffer[ROM_BUFFER_LEN];
const char *filename = "rom.bin";
eMode epromMode = m27C020;
bool lynxMode = false;
Stream* stream;

int32_t inPins[] = { 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27,2,3,4,33, -1 };
int32_t outPins[] = { 10,12,11,13,8,7,36,37, -1 };

void readData(Stream* s)
{
	const int length = 1024;
	char buff[length];
	int total = 0;

	epromMode = (eMode)s->read();
	lynxMode  = (bool)s->read();

	memset(buffer, 0xFF, ROM_BUFFER_LEN);

	size_t read = 0;
	char* p = buffer;

	do
	{
		memset(buff, 0, length);

		read = s->readBytes(buff, length);

		if(read > 0)
		{
			memcpy(p, buff, read);
			p += read;
			total += read;
		}
	}
	while(read != 0);

	SD.remove(filename);

	File f = SD.open(filename, O_WRITE);
	f.write(epromMode);
	f.write(lynxMode);
	f.write(buffer, total);
	f.close();
}

void setPinMode(int32_t* pins, int32_t direction)
{
	for(int i = 0; pins[i] != -1; i++)
		pinMode(pins[i], direction);
}

void setup()
{
	epromMode = m27C256;

	setPinMode(inPins, INPUT);
	setPinMode(outPins, OUTPUT);
	GPIO7_DR = 0;

	SD.begin(BUILTIN_SDCARD);

	if(SD.exists(filename))
	{
		File f = SD.open(filename, O_READ);
		size_t size = f.size();
		epromMode = (eMode)f.read();
		lynxMode = f.read();

		f.read(buffer, size);
		f.close();
	}

	Serial.begin(115200);
}

void loop()
{
	uint32_t io6 = GPIO6_DR;
	uint32_t io9 = GPIO9_DR;

	uint32_t outb = 0;

	// read address pins
	uint32_t addr = 0;

	switch(epromMode)
	{
		// 256Kbit, 32KB, 0x0000-0x7FFF
		case m27C256:
			addr = ((io6 >> 16) & 0x7FFF);
			break;

		// 1Mbit, 128KB, 0x00000-0x1FFFF
		case m27C010:
			if(lynxMode)
			{
				addr =   (io6 >> 16) & 0x001FF;  // Lynx A0 - A8   -> EPROM A0 - A8
				addr |=  (io6 >> 18) & 0x03E00;  // Lynx A12 - A17 -> EPROM A11 - A15
				addr |= ((io9 << 10) & 0x1C000); // Lynx A18 - A19 -> EPROM A16
			}
			else
				addr = ((io6 >> 16) & 0xFFFF) | ((io9 << 12) & 0x10000);
			break;

		// 2Mbit, 256KB, 0x00000-0x3FFFF
		case m27C020:
		default:
			if(lynxMode)
			{
				addr =   (io6 >> 16) & 0x003FF;  // Lynx A0 - A9   -> EPROM A0 - A9
				addr |=  (io6 >> 17) & 0x07C00;  // Lynx A12 - A17 -> EPROM A11 - A15
				addr |= ((io9 << 11) & 0x38000); // Lynx A18 - A19 -> EPROM A16 - A17
			}
			else
				addr = ((io6 >> 16) & 0xFFFF) | ((io9 << 12) & 0x30000);
			break;

		// 4Mbit, 512KB, 0x00000-0x7FFFF
		case m27C040:
			addr = ((io6 >> 16) & 0xFFFF) | ((io9 << 12) & 0x70000);
			break;

		// 8Mbit, 1MB, 0x00000-0xFFFFF
		case m27C080:
			addr = ((io6 >> 16) & 0xFFFF) | ((io9 << 12) & 0xF0000);
			break;

	}

	// get byte at addr
	char b = buffer[addr];

	// set data pins
	outb = ((b & 0x0F) << 0) | ((b & 0xF0) << 12);
	GPIO7_DR = outb;

	// read file from either serial port, if available
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