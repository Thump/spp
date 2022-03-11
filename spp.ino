/* pixel-gadget non-cloud version
 *
 * Brad Black - 2021
 * 
 * Based on DemoReel100 by Mark Kriegsman and as modified by Andrew Tuline for button support
 * 
 * This code only shows variations of the confetti demo and supports a rotary encoder (EC11 style) with switch to change 
 * parameters and switch modes.  Pixel Gadget is intended to be an 8x8 matrix of WS2812 LEDs.
 *
 * Instructions:
 * 
 * Program reads display mode + options from EEPROM and displays it.
 * Button Option:
 *  - Click to change to the next color
 *  - Hold button for > 1 second to shut off screen and write the current mode to EEPROM.
 *  - Double click to change intensity
 * 
 * Rotary Encoder Option:
 * 
 *  - single click = cycle modes: { color, intensity, brightness, auto change }
 *  - double click = save for restart
 *  - long press = screen off
 * 
 */
//===============
// Curent issues
//               NONE
//================

#include <EEPROM.h>

const char *project_info = "\nPIXEL GADGET - VERSION 2\nDEC 11, 2021\nBRAD BLACK\n";

#define NUM_LEDS 64      // Number of LED's.
uint8_t max_bright = 40; // Overall brightness definition. It can be changed on the fly.

#define USING_ENCODER true; // Comment out if using button instead of rotary encoder

#ifdef ESP32
#define buttonPin 17 // Digital pin connected sensor
#define LED_DT 16    // Data pin to connect to the strip.  Use 4 for ESP8266 and 16 for ESP32
#endif

#if defined(ESP8266)

#define buttonPin 2     //D4 // Digital pin connected sensor 2 for Wemos D1, 0 for "Lua" Board onboard switch
#define LED_DT 4        //D2 // Data pin to connect to the strip.  Use 4 for ESP8266 and 16 for ESP32
#define encoder0PinA 12 //D5 was 14
#define encoder0PinB 14 //D6 was 12

#endif

#include <FastLED.h>
//#define FASTLED_INTERRUPT_RETRY_COUNT 0
//#define FASTLED_ALLOW_INTERRUPTS 0
#include "jsbutton.h" // Nice button routine by Jeff Saltzman

// Fixed definitions cannot change on the fly.

#define COLOR_ORDER BGR   // It's GRB for WS2812 and BGR for APA102.
#define LED_TYPE NEOPIXEL // Using APA102, WS2812, WS2801. Don't forget to modify LEDS.addLeds to suit.

// Definition for the array of routines to display.
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

//Mode and EEPROM variables
uint8_t maxMode = 6; // Maximum number of display modes. Would prefer to get this another way, but whatever.
int eepaddress = 0;

// Global variables can be changed on the fly.

struct CRGB leds[NUM_LEDS]; // Initialize our LED array.

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns
uint8_t gIntensity[] = {10, 30, 60, 80, 100};
uint8_t gCurrentIntensity = 1;

enum mode                                                   // available modes of operation of the gadget
{
  pattern,
  intensity,
  brightness,
  auto_change,
  number_of_modes
};

int current_mode = pattern;
int current_auto_change = 1;
int current_brightness = 5;            // using int between 1-4 for encoder - these will be multiplied by 8 for actual FastLED brightness
typedef void (*SimplePatternList[])(); // List of patterns to cycle through.  Each is defined as a separate function below.

bool SLEEPING = false;
int value_override = 0;                // used to over-ride the current brightness value when "turning off" the display gradually
int timer;                             // keep track of time for auto_change

int encoder0Pos = 1;
int oldEncoder0Pos = 1;

int pinA;
int pinB;

const uint64_t IMAGES[] = {            // image patterns for the mode indicator when you press the button
    // https://xantorohara.github.io/led-matrix-editor/#

    0x0066666666666600,
    0x000c3c7c7c3c0c00,
    0x00247e24247e2400, // # = intensity or number of pixels??
    0x004a187c3e185200, //brightness
    0x00040e1a30604000  //check for save

};
const int IMAGES_LEN = sizeof(IMAGES) / 8;
enum images
{
  auto_change_off,
  auto_change_on,
  intense,
  brite,
  check
};


void setup()
{
  EEPROM.begin(32);
  Serial.begin(57600); // Initialize serial port for debugging.
  delay(1000);
  Serial.print(project_info);

  pinMode(encoder0PinA, INPUT_PULLUP);
  digitalWrite(encoder0PinA, HIGH);

  pinMode(encoder0PinB, INPUT_PULLUP);
  digitalWrite(encoder0PinB, HIGH);
  pinMode(buttonPin, INPUT_PULLUP); // Set button input pin
  digitalWrite(buttonPin, HIGH);

  LEDS.addLeds<LED_TYPE, LED_DT>(leds, NUM_LEDS); // Use this for WS2812

  gCurrentPatternNumber = EEPROM.read(eepaddress);
  gCurrentIntensity = EEPROM.read(eepaddress + 4);
  current_brightness = EEPROM.read(eepaddress + 8);
  current_auto_change = EEPROM.read(eepaddress + 12);

  if (gCurrentPatternNumber > maxMode)
    gCurrentPatternNumber = 0; // A safety in case the EEPROM has an illegal value.
  if (gCurrentIntensity > ARRAY_SIZE(gIntensity))
    gCurrentIntensity = 1;
  if (current_brightness < 1)
    current_brightness = 1;
  if (current_brightness > 5)
    current_brightness = 5;

  if ((current_brightness * 8) > max_bright)
    current_brightness = 5;
  FastLED.setBrightness(current_brightness * 8);

  if (current_auto_change > 1)
    current_auto_change = 1;
  if (current_auto_change < 0)
    current_auto_change = 0;

  //  FastLED.setMaxPowerInVoltsAndMilliamps(3.3, 500);               // FastLED Power management set at 5V, 500mA.

  Serial.print("Start Pattern: ");
  Serial.println(gCurrentPatternNumber);
  timer = millis();
} // setup()

SimplePatternList gPatterns = {christmas, confetti, confetti_green, confetti_blue, confetti_purple, confetti_red, confetti_yellow, darkness};

void loop()
{

  if (current_auto_change == 1)
  {
    if (millis() - timer > 900000)                         // Change color every 15 minutes when auto_change is enabled
    {
      gCurrentPatternNumber = (gCurrentPatternNumber + 1) % (ARRAY_SIZE(gPatterns) - 1);
      Serial.printf("Timer changed pattern to: %i", gCurrentPatternNumber);
      timer = millis();
      current_mode = pattern;
    }
  }

#ifdef USING_ENCODER
  readbutton_encoder();
  doBBEncoder();

  if (encoder0Pos != oldEncoder0Pos)
  {

    switch (current_mode)
    {
    case pattern:

      if (encoder0Pos < 0)
        encoder0Pos = (ARRAY_SIZE(gPatterns) - 2);
      if (encoder0Pos > (ARRAY_SIZE(gPatterns) - 2))
        encoder0Pos = 0;
      gCurrentPatternNumber = encoder0Pos;
      Serial.printf("Pattern: %i\n", encoder0Pos);
      oldEncoder0Pos = encoder0Pos;
      break;
    case intensity:

      if (encoder0Pos < 0)
        encoder0Pos = (ARRAY_SIZE(gIntensity) - 1);
      if (encoder0Pos > (ARRAY_SIZE(gIntensity) - 1))
        encoder0Pos = 0;
      gCurrentIntensity = encoder0Pos;
      Serial.printf("Itensity: %i\n", encoder0Pos);
      oldEncoder0Pos = encoder0Pos;
      break;
    case brightness:

      if (encoder0Pos < 1)
        encoder0Pos = 1;
      if (encoder0Pos > 5)
        encoder0Pos = 5;
      current_brightness = encoder0Pos;
      Serial.printf("Setting Brightness to : %i  Actual brightness will be %i\n", current_brightness, current_brightness * 8);
      if ((current_brightness * 8) > max_bright)
        current_brightness = 5;
      FastLED.setBrightness(current_brightness * 8);
      oldEncoder0Pos = encoder0Pos;
      break;

    case auto_change:

      if (encoder0Pos < 0)
        encoder0Pos = 0;
      if (encoder0Pos > 1)
        encoder0Pos = 1;
      current_auto_change = encoder0Pos;
      Serial.printf("Setting AutoChange to : %i\n", current_auto_change);
      oldEncoder0Pos = encoder0Pos;
      displayImage(IMAGES[current_auto_change]);
      break;
    }
  }

#else
  readbutton();
#endif

  EVERY_N_MILLISECONDS(50)
  {                                     
    gPatterns[gCurrentPatternNumber](); // Call the current pattern function once, updating the 'leds' array
  }

  EVERY_N_MILLISECONDS(20)
  { // slowly cycle the "base color" through the rainbow
    gHue++;
  }
  if (!SLEEPING)
    FastLED.show();

} // loop()

void readbutton_encoder()
{ // Read the button and increase the mode

  uint8_t b = checkButton();

  if (b == 1)
  { // Just a click  wake from sleep or change mode

    if (SLEEPING)
    {
      gCurrentPatternNumber = EEPROM.read(eepaddress);
      SLEEPING = false;
      value_override = 0;
      current_mode = pattern;
      Serial.println("Woke from Sleep");
    }
    else
    {

      current_mode = (current_mode + 1) % (number_of_modes);

      switch (current_mode)
      {
      case pattern:

        encoder0Pos = gCurrentPatternNumber;
        displayRainbow();
        break;
      case intensity:

        encoder0Pos = gCurrentIntensity;
        displayImage(IMAGES[intense]);
        break;

      case brightness:

        encoder0Pos = current_brightness;
        displayImage(IMAGES[brite]);
        break;

      case auto_change:

        encoder0Pos = current_auto_change;
        displayImage(IMAGES[current_auto_change]);
        break;
      }
      Serial.printf("Changed mode to %i - Pattern: %i - Intensity: %i - Brightness: %i - Autochange: %i - EncoderPos: %i\n", current_mode, gCurrentPatternNumber, gCurrentIntensity, current_brightness, current_auto_change, encoder0Pos);
    }
  }

  if (b == 2)
  { // A double-click event to save initial pattern

    EEPROM.write(eepaddress, gCurrentPatternNumber);
    EEPROM.write(eepaddress + 4, gCurrentIntensity);
    EEPROM.write(eepaddress + 8, current_brightness);
    EEPROM.write(eepaddress + 12, current_auto_change);
    EEPROM.commit();
    displayImage(IMAGES[4]);
    Serial.printf("Writing: Pattern: %i Intensity: %i  Brightness: %i Autochange: %i\n", gCurrentPatternNumber, gCurrentIntensity, current_brightness, current_auto_change);
  }

  if (b == 3)
  { // A hold event turn off the display

    SLEEPING = true;

    for (int i = 32; i < 256; i = i + 3)
    {
      value_override = i;
      gPatterns[gCurrentPatternNumber]();
      FastLED.show();
      delay(50);
    }
    fadeToBlackBy(leds, NUM_LEDS, 50);
    delay(50);
    FastLED.show();
    fadeToBlackBy(leds, NUM_LEDS, 50);
    delay(50);
    FastLED.show();
    FastLED.clear();
    FastLED.show();
    gCurrentPatternNumber = ARRAY_SIZE(gPatterns) - 1;
  }

} // readbutton_encoder()

void readbutton()
{ // Read the button and increase the mode

  uint8_t b = checkButton();

  if (b == 1)
  { // Just a click event to advance to next pattern

    if (SLEEPING)
    {
      gCurrentPatternNumber = EEPROM.read(eepaddress);
      SLEEPING = false;
      value_override = 0;
      Serial.println("Woke from Sleep");
    }
    else
    {

      gCurrentPatternNumber = (gCurrentPatternNumber + 1) % (ARRAY_SIZE(gPatterns) - 1);
      Serial.println(gCurrentPatternNumber);
    }
  }

  if (b == 2)
  { // A double-click event to change intensity
    //gCurrentPatternNumber = 0;

    gCurrentIntensity = (gCurrentIntensity + 1) % ARRAY_SIZE(gIntensity);
    Serial.println(gCurrentIntensity);
  }

  if (b == 3)
  { // A hold event to turn off the display

     SLEEPING = true;

    for (int i = 32; i < 256; i = i + 3)
    {
      value_override = i;
      gPatterns[gCurrentPatternNumber]();
      FastLED.show();
      delay(50);
    }
    fadeToBlackBy(leds, NUM_LEDS, 50);
    delay(50);
    FastLED.show();
    fadeToBlackBy(leds, NUM_LEDS, 50);
    delay(50);
    FastLED.show();
    FastLED.clear();
    FastLED.show();
   
    EEPROM.write(eepaddress,gCurrentPatternNumber);
    EEPROM.write(eepaddress+4,gCurrentIntensity);
    EEPROM.commit();
    Serial.print("Writing: ");
    Serial.println(gCurrentPatternNumber);    

    gCurrentPatternNumber = ARRAY_SIZE(gPatterns) - 1;

  }

} // readbutton()

void doBBEncoder()
{
  if (digitalRead(encoder0PinB) == LOW)
  {
    pinA = !digitalRead(encoder0PinA);

    if (pinA == 0)
    {
      --encoder0Pos;
    }

    delay(80);
  }

  if (digitalRead(encoder0PinA) == LOW)
  {
    pinB = !digitalRead(encoder0PinB);

    if (pinB == 0)
    {
      ++encoder0Pos;
    }

    delay(80);
  }
}

void displayImage(uint64_t image)
{
  for (int i = 0; i < 8; i++)
  {
    byte row = (image >> i * 8) & 0xFF;
    for (int j = 0; j < 8; j++)
    {

      leds[i * 8 + j] = CRGB(75 * bitRead(row, j), 75 * bitRead(row, j), 75 * bitRead(row, j));
    }
  }
}

void displayRainbow()
{
  for (int i = 1; i < 7; i++)
  {

    for (int j = 1; j < 7; j++)
    {

      leds[i * 8 + j] = CHSV(((i + 2) * 16) + ((j + 2) * 16), 255, 255);
    }
  }
}

//--------------------[ Effects are below here ]------------------------------------------------------------------------------

void darkness()
{
}
void rainbow()
{

  fill_rainbow(leds, NUM_LEDS, gHue, 7); // FastLED's built-in rainbow generator.

} // rainbow()

void rainbowWithGlitter()
{

  rainbow(); // Built-in FastLED rainbow, plus some random sparkly glitter.
  addGlitter(80);

} // rainbowWithGlitter()

void addGlitter(fract8 chanceOfGlitter)
{

  if (random8() < chanceOfGlitter)
  {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }

} // addGlitter()

void confetti()
{ // Random colored speckles that blink in and fade smoothly.

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255 - value_override);

} // confetti()

void christmas()
{ // Random colored speckles that blink in and fade smoothly.

  int xmas[] = {0, 32, 96, 160};
  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] = CHSV(xmas[random8(4)], 255, 255 - value_override);

} // confetti()

void confetti_slow()
{ // Random colored speckles that blink in and fade smoothly.

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(gHue / 4 + random8(32), 200, 255 - value_override);

} // confetti()

void confetti_green()
{

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(random8(40) + 80, 255, 255 - value_override);
}

void confetti_blue()
{

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(random8(40) + 145, 255, 255 - value_override);
}

void confetti_purple()
{

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(random8(40) + 180, 255, 255 - value_override);
}

void confetti_red()
{

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(random8(32) - 16, 255, 255 - value_override);
}

void confetti_yellow()
{

  fadeToBlackBy(leds, NUM_LEDS, gIntensity[gCurrentIntensity]);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(random8(32) + 40, 255, 255 - value_override);
}
