
#include <string>
#include <vector>

#define PREFER_SDFAT_LIBRARY
#include <SPI.h>
#include <SdFat.h>                // SD card & FAT filesystem library
#include <Adafruit_GFX.h>         // Core graphics library
#include <Adafruit_SSD1351.h>     // Hardware-specific library
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <Chrono.h>
#include <Bounce2.h>
#include <Adafruit_VS1053.h>
#include <MFRC522.h>
#include <Wire.h>
#include "Adafruit_TPA2016.h"

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(s)		Serial.print(s)
#define DEBUG_PRINTLN(s)	Serial.println(s)
#else
#define DEBUG_PRINT(s)
#define DEBUG_PRINTLN(s)
#endif

#define DEBUG_PRINT_VAR(text, var)	DEBUG_PRINT(text); DEBUG_PRINT(": "); DEBUG_PRINTLN(var)

const char DELIMITER = ';';
const char* SHUFFLE_FILE = "_shuffle";
const char* COVER_FILE = "_cover.bmp";

const int PIN_SDCARD_CS = 10;
const int PIN_VS1053_CS = 11;
const int PIN_VS1053_DCS = 12;
const int PIN_VS1053_DREQ = 1;
const int PIN_VS1053_RST = -1;

const int PIN_OLED_CS = 7;
const int PIN_OLED_DC = 9;
const int PIN_OLED_RST = -1;

const int PIN_MFRC522_CS = 3;
const int PIN_MFRC522_RST = 4;
const int PIN_MFRC522_IRQ = 0;

const int PIN_BUTTON_PLAY = A3;
const int PIN_BUTTON_NEXT = A2;
const int PIN_BUTTON_PREV = A4;
const int PIN_POT_VOLUME = A1;

const int PIN_AMP_SD = 2;

const int PIN_PLUG_DETECT = A5;

const int PIN_SHUTDOWN = 13;

const int PIN_BATTERY_PROBE = A0;

#define RGB565(RGB88) (((RGB88&0xf80000)>>8) + ((RGB88&0xfc00)>>5) + ((RGB88&0xf8)>>3))
const uint16_t COLOR_BLACK = RGB565(0x0);
const uint16_t COLOR_WHITE = RGB565(0x0);
const uint16_t COLOR_BACKGROUND = COLOR_BLACK;
const uint16_t COLOR_TEXT = COLOR_WHITE;

// Metros
Chrono  chronoCheckBattery = Chrono(Chrono::MILLIS);
Chrono  chronoShutDown = Chrono(Chrono::SECONDS);
Chrono  chronoResetCH4 = Chrono(Chrono::MILLIS);

// Buttons
Bounce buttonPlay = Bounce();
Bounce buttonPrev = Bounce();
Bounce buttonNext = Bounce();
Bounce plugDetect = Bounce();
Bounce buttons[4] = { buttonPlay, buttonPrev, buttonNext, plugDetect };

// SD
SdFat                SD;         // SD card filesystem
Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys

// Display
const unsigned int SCREEN_WIDTH = 128;
const unsigned int SCREEN_HEIGHT = 128;
Adafruit_SSD1351 display = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);

// Music
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(PIN_VS1053_RST, PIN_VS1053_CS, PIN_VS1053_DCS, PIN_VS1053_DREQ, PIN_SDCARD_CS);

// AMP
Adafruit_TPA2016 audioamp = Adafruit_TPA2016();


// RFID
MFRC522 rfid(PIN_MFRC522_CS, PIN_MFRC522_RST); // Instance of the class
MFRC522::MIFARE_Key rfidKey;
byte nuidPICC[4];

// State
bool playing = false;
bool isAudioBook = false;
bool isSingleFile = false;
bool headphonePluggedIn = false;

std::string currentPathDirectory("");
std::string currentMP3("");
std::string currentCover("");
File currentDirectory;
File sdRoot;
File artwork;
std::vector<std::string> playlist;
long currentPlaylistIndex = 0;

int volume = 255;

typedef struct
{
	//std::string card;
	byte card[4];
	std::string file;
} Action;

std::vector<Action> actions;

std::string dirname(std::string file)
{
	size_t found = file.find_last_of("/\\");
	return file.substr(0,found);
}

int isBatteryGood(bool ignoreTimer = false)
{
  /*
     V+ --- 1M --- 1M --- 470k --- GND
                       |
                  PIN_V_PROBE

     9.6V * 470k / (1M + 1M + 470k) = 1.82V
       6V * 470k / (1M + 1M + 470k) = 1.14V

     VRef = 3.3V
     1.82 / 3.3 * 1024 - 1: 565
     1.14 / 3.3 * 1024 - 1: 354
  */

	if (ignoreTimer || chronoCheckBattery.hasPassed(5000, true))
	{
		if (analogRead(PIN_BATTERY_PROBE) < 400)
			return 0;
	}
	return 1;
}

void display_text(const char* string, unsigned int x, unsigned int y, unsigned int size = 1, uint16_t color = COLOR_WHITE)
{
	display.setTextColor(color);
	display.setCursor(x, y);
	display.setTextSize(size);

	display.println(string);
}

void display_init()
{
	display_clear();
	display_text("Musikschlumpf", 4, 48, 3, COLOR_WHITE);
}

void display_hello()
{
	display_clear();
	display_text("Musikschlumpf ist bereit", 4, 48, 3, COLOR_WHITE);
}

void display_clear()
{
	display.fillScreen(COLOR_BACKGROUND);
}

void display_status()
{
	display.fillRect(0,0, 127, 10, COLOR_BLACK);
	if(playing)
		display_text("play", 9, 2, 2, COLOR_WHITE);
}

void setupDisplay()
{
	DEBUG_PRINT("setup Display... ");

	display.begin();
	display_init();

	DEBUG_PRINTLN("done");
}

void setupSD()
{
	DEBUG_PRINT("setup SD... ");

	if(!SD.begin(PIN_SDCARD_CS, SD_SCK_MHZ(10))) { // Breakouts require 10 MHz limit due to longer wires
		Serial.println(F("SD begin() failed"));
		for(;;); // Fatal error, do not continue
	}

	sdRoot = SD.open("/");

	DEBUG_PRINTLN("done");
}

void setupMusic()
{
	DEBUG_PRINT("setup Music... ");

	if (! musicPlayer.begin()) { // initialise the music player
		Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
		while (1);
	}

	Serial.println(F("VS1053 found"));

	// list files
	//printDirectory(SD.open("/"), 0);

	// Set volume for left, right channels. lower numbers == louder volume!
	musicPlayer.setVolume(volume, volume);
	musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

	DEBUG_PRINTLN("done");
}

void setupAMP()
{
	DEBUG_PRINT("setup AMP... ");

	pinMode(PIN_AMP_SD, OUTPUT);
	digitalWrite(PIN_AMP_SD, HIGH);

	audioamp.begin();
	audioamp.enableChannel(true, true);
	audioamp.setAGCCompression(TPA2016_AGC_2);

	DEBUG_PRINTLN("done");
}

void setupRFID()
{
	DEBUG_PRINT("setup RFID... ");

	SPI.begin(); // Init SPI bus
	rfid.PCD_Init(); // Init MFRC522 

	for (byte i = 0; i < 6; i++) {
		rfidKey.keyByte[i] = 0xFF;
	}

	Serial.println(F("This code scan the MIFARE Classsic NUID."));
	Serial.print(F("Using the following key:"));
	printHex(rfidKey.keyByte, MFRC522::MF_KEY_SIZE);

	DEBUG_PRINTLN("done");
}

void setupRandomizer()
{
	randomSeed(analogRead(PIN_BATTERY_PROBE) + analogRead(PIN_POT_VOLUME));
}

void setupButtonsAndVolume()
{
	pinMode(PIN_BUTTON_PLAY, INPUT_PULLUP);
	pinMode(PIN_BUTTON_NEXT, INPUT_PULLUP);
	pinMode(PIN_BUTTON_PREV, INPUT_PULLUP);
	pinMode(PIN_PLUG_DETECT, INPUT_PULLUP);

	buttonPlay.attach(PIN_BUTTON_PLAY);
	buttonPlay.interval(25);
	buttonPrev.attach(PIN_BUTTON_PREV);
	buttonPrev.interval(25);
	buttonNext.attach(PIN_BUTTON_NEXT);
	buttonNext.interval(25);
	plugDetect.attach(PIN_PLUG_DETECT);
	plugDetect.interval(25);

	// PIN_POT_VOLUME
	analogReadResolution(10);
}

void setup()
{
	pinMode(PIN_SHUTDOWN, OUTPUT);
	digitalWrite(PIN_SHUTDOWN, LOW);

	Serial.begin(115200);
	while (!Serial) { delay(1); }

	DEBUG_PRINTLN("setup musikschlumpf... ");

	setupDisplay();
	setupAMP();
	setupSD();
	setupActions();
	setupMusic();
	setupButtonsAndVolume();
	setupRFID();

	setupRandomizer();

	display_hello();

	DEBUG_PRINTLN("setup musikschlumpf... done");
}

void checkButtons()
{
	bool action = false;
	buttonPlay.update();
	buttonPrev.update();
	buttonNext.update();

	if(buttonPlay.read() == LOW)
	{
		if (! musicPlayer.paused()) {
			DEBUG_PRINTLN("Paused");
			musicPlayer.pausePlaying(true);
		} else { 
			DEBUG_PRINTLN("Resumed");
			musicPlayer.pausePlaying(false);
		}
		playing = musicPlayer.paused();

		action = true;
	}
	if(buttonNext.read() == LOW)
	{
		playNext();
		action = true;
	}
	if(buttonPrev.read() == LOW)
	{
		musicPlayer.stopPlaying();
		delay(100);
		play(currentMP3);
		
		action = true;
	}

	if(action)
		chronoShutDown.restart(0);
}

void play(std::string file)
{
	if(musicPlayer.isMP3File(file.c_str()))
	{
		musicPlayer.startPlayingFile(file.c_str());
		currentMP3 = file;

		DEBUG_PRINT_VAR("playing file: ", file.c_str());
	}
	else
	{
		DEBUG_PRINT_VAR("not a valid mp3 file", file.c_str());
	}
}

std::string getRandomFile()
{
	currentPlaylistIndex = random(0, playlist.size()-1);
	return playlist[currentPlaylistIndex];
}

std::string getNextFile()
{
	currentPlaylistIndex = (currentPlaylistIndex + 1) % playlist.size();
	return playlist[currentPlaylistIndex];
}

void playNext()
{
	std::string mp3;
	if(isShuffleDir(currentDirectory))
		mp3 = getRandomFile();
	else
		mp3 = getNextFile();

	play(mp3);
}

bool isShuffleDir(File directory)
{
	return directory.exists(SHUFFLE_FILE);
}

bool compare(byte a[4], byte b[4])
{
	return  a[0] == b[0] &&
			a[1] == b[1] &&
			a[2] == b[2] &&
			a[3] == b[3];
}

void generatePlaylist(std::string fullDirectoryPath, File directory)
{
	playlist.clear();
	currentPlaylistIndex = 0;

	File entry;
	while(entry = directory.openNextFile())
	{
		if(!entry.isFile())
			continue;

		size_t NAME_LEN = 13;
		char name[NAME_LEN];
		entry.getName(name, NAME_LEN);
		if(name[0] == '_')
			continue;

		std::string fullFilePath = "/";
		fullFilePath.append(fullDirectoryPath).append("/").append(name);

		if(musicPlayer.isMP3File(fullFilePath.c_str()))
			playlist.push_back(fullFilePath);
	}

	#ifdef DEBUG
	DEBUG_PRINTLN("playlist:");
	for(auto &s: playlist)
		DEBUG_PRINTLN(s.c_str());
	#endif
}

void playByNewCard()
{
	for(auto &action: actions)
	{
		if(compare(action.card, nuidPICC))
		{
			currentPathDirectory = action.file;
			if(sdRoot.exists(currentPathDirectory.c_str()))
			{
				currentDirectory.close();
				sdRoot.open(&currentDirectory, currentPathDirectory.c_str(), O_RDONLY);
				generatePlaylist(currentPathDirectory, currentDirectory);
				displayCover(currentPathDirectory, currentDirectory);
				playNext();
			}
			return;
		}
	}
	#ifdef DEBUG
	DEBUG_PRINT("unknown new card ");
	for(int i = 0; i < 4; ++i)
		DEBUG_PRINT(nuidPICC[i]);
	DEBUG_PRINTLN(" :(");
	#endif
}

void displayCover(std::string fullDirectoryPath, File directory)
{
	if(directory.exists(COVER_FILE))
	{
		currentCover = "/";
		currentCover.append(fullDirectoryPath).append("/").append(COVER_FILE);

		char *cover_cstr = new char[currentCover.length() + 1];
		strcpy(cover_cstr, currentCover.c_str());
		ImageReturnCode stat = reader.drawBMP(cover_cstr, display, 0, 0);
		delete[] cover_cstr;

		if(stat != IMAGE_SUCCESS)
		{
			DEBUG_PRINTLN("failed to display cover");
		}
	}
	else
		display_text(fullDirectoryPath.c_str(), 3, 32);
}

void checkHeadphonePlugAndVolume()
{
	plugDetect.update();
	int v = analogRead(PIN_POT_VOLUME) / 4;

	if(abs(v-volume) > 5)
	{
		volume = v;
		musicPlayer.setVolume(volume, volume);
	}

	bool plugDetected = plugDetect.read() == HIGH;
	if(plugDetected != headphonePluggedIn)
	{
		headphonePluggedIn = plugDetected;
	
		// plugged in = HEADPHONES
		if(headphonePluggedIn)
		{
			audioamp.enableChannel(false, false);
		}
		// not plugged in = SPEAKERS
		else
		{
			audioamp.enableChannel(true, true);
		}
	}
}

void loop()
{
	checkButtons();
	checkHeadphonePlugAndVolume();

	isBatteryGood();

	if(checkRFIDForNewCard())
	{
		playByNewCard();
	}

	if(chronoShutDown.hasPassed(2*60*60))
	{
		DEBUG_PRINTLN("goodbye");
		digitalWrite(PIN_SHUTDOWN, HIGH);
	}

	delay(500);
}

bool checkRFIDForNewCard()
{
	  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent())
    return false;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return false;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return false;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || 
    rfid.uid.uidByte[1] != nuidPICC[1] || 
    rfid.uid.uidByte[2] != nuidPICC[2] || 
    rfid.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
   
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  }
  else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

  return true;
}

byte charToByte(char c)
{
	if(c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'A' && c <= 'F') 
		return (10 + (c - 'A'));
	else if (c >= 'a' && c <= 'f')
		return (10 + (c - 'a'));

	DEBUG_PRINT_VAR("charToByte unkown char", c);

	return 0;
}

void setupActions()
{
	File fileActions = SD.open("musikschlumpf.txt");
	if (fileActions)
	{
		bool skipLine = false;
		const unsigned int BUFFER_SIZE = 64;
		char buffer[BUFFER_SIZE];
		unsigned int index = 0;
		const unsigned int READ_CARD = 0;
		const unsigned int READ_FILE = 1;
		unsigned int READ = READ_CARD;

		Action action;

	    while (fileActions.available())
	    {
	    	unsigned char c = fileActions.read();
	    	if(c == '\n' && skipLine)
	    	{
	    		skipLine = false;
	    		READ = READ_CARD;
	    		index = 0;
    			action.card[0] = 0x0b;
    			action.card[1] = 0xad;
    			action.card[2] = 0xf0;
    			action.card[3] = 0x0d;
    			action.file = "";
	    	}
	    	else if(!skipLine)
	    	{
	    		if(c == '\n')
	    		{
	    			action.file.assign(buffer, index);
	    			actions.push_back(action);
	    			READ = READ_CARD;
	    			index = 0;
	    		}
	    		else if(c == DELIMITER)
	    		{
	    			for(unsigned int i = 0; i < 4; ++i)
	    				action.card[i] = (charToByte(buffer[2*i+0]) << 4) + (charToByte(buffer[2*i+0]));
	    			READ++;
	    		}

	    		if(index < BUFFER_SIZE)
	    		{
	    			buffer[index] = c;
	    			++index;
	    		}
	    	}
	    }
    }
    fileActions.close();

#ifdef DEBUG
    Serial.println("I parsed the following actions:");
	for(const auto& action: actions) {
	    Serial.printf("  %c:%c:%c:%c : %s\n", action.card[0], action.card[1], action.card[2], action.card[3], action.file.c_str());
	}
	Serial.println("");
#endif
}

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

/*
 #include <SD.h>
#include <SPI.h>


void setup()
{

    Serial.begin(9600);
    while (!Serial);

    Serial.print("Initializing SD card...");
    if (!SD.begin(53)) {
        Serial.println("initialization failed!");
        while (1); // <- this is how you should block execution, not with returns
    }
    Serial.println("initialization done.");


    // Open
    File bmpImage = SD.open("picture.bmp", FILE_READ);
    textFile = SD.open("test.txt", FILE_WRITE);

    int32_t dataStartingOffset = readNbytesInt(&bmpImage, 0x0A, 4);

    // Change their types to int32_t (4byte)
    int32_t width = readNbytesInt(&bmpImage, 0x12, 4);
    int32_t height = readNbytesInt(&bmpImage, 0x16, 4);
    Serial.println(width);
    Serial.println(height);

    int pixelsize = readNbytesInt(&bmpImage, 0x1C, 2);

    if (pixelsize != 24)
    {
        Serial.println("Image is not 24 bpp");
        while (1);
    }

    //skip bitmap header
     bmpImage.seek(dataStartingOffset);
    // 24bpp means you have three bytes per pixel, usually B G R

    byte RED, GREEN, BLUE;
    int R[height][width];
    int G[height][width];
    int B[height][width];

    for(int i = 0; i < height; i ++) {
      byte a;
      if(i>=3)
        {
          a = bmpImage.read();
        }

     for (int j = 0; j < width; j ++) {

            BLUE = bmpImage.read();
            GREEN = bmpImage.read();
            RED = bmpImage.read();

            R[height-1-i][j] = RED;
            G[height-1-i][j] = GREEN;
            B[height-1-i][j] = BLUE;
        }
        a = bmpImage.read();
        a = bmpImage.read();
    }
    bmpImage.close();
}

void loop(){}

int32_t readNbytesInt(File *p_file, int position, byte nBytes)
{
    if (nBytes > 4)
        return 0;

    p_file->seek(position);

    int32_t weight = 1;
    int32_t result = 0;
    for (; nBytes; nBytes--)
    {
        result += weight * p_file->read();
        weight <<= 8;
    }
    return result;
}


*/