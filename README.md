# musikschlumpf

## functions

## parts list
 * adafruit itsy bitsy m4
 * adafruit amp
 * adafruit mp3 shield with SD card
 * adafruit soft power button
 * headphone jack
 * voltage converter
 * speakers
 * some cables, pins and headers

## errata
Feather board miso/mosi pin are switched :/
Never use pin13 for something which should not be high on boot :/
you might want to change the SD usage in Adafruit_vs1053 as attached in Schlumpf_Adafruit_VS1053, see startPlayingFile