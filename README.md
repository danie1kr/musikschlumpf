# musikschlumpf

## functions

The musikschlumpf plays MP3 files from directories selected by a RFID card.
On the display, images can be displayed per directory or per MP3 file played.

Buttons allow play/pause, next/prev and on/off. With the soft power button, the can power-off itself after no interaction for a defined period of time.

Plugging in a head phone disables the speakers.

In summary, the musikschlumpf is simlar to a [TonUINO](https://www.voss.earth/tonuino/) but as there are more advanced requirements (cover art, headphone functionality), the result was a completly different approach.

The schematics can be found [here](doc/musikschlumpf_schematics.pdf) for reference.

## parts list
 * [adafruit itsy bitsy m4](https://www.adafruit.com/product/3800)
 * [adafruit amp with TPA2016](https://www.adafruit.com/product/1712)
 * [adafruit mp3 shield with SD card](https://www.adafruit.com/product/3357)
 * [adafruit soft power button](https://www.adafruit.com/product/1400)
 * Waveshare 1.5 Inch RGB OLED
 * AZDelivery RFID Kit RC522
 * headphone jack
 * voltage converter, 9V to 5V and 3.3V
 * speakers
 * some cables, pins and headers

## errata :/
Feather board miso/mosi pins are switched

Never use pin13 for something which should not be high on boot

you might want to change the SD usage in Adafruit_vs1053 as attached in Schlumpf_Adafruit_VS1053, see startPlayingFile