/* spp
 *
 *  Changelog
 *
 *  Friday, March 11, 2022
 *   Denis McLaughlin
 *   - initial Makefile implementation, based on Brad Black's pixel cloud code
 *
 *  Monday, March 14, 2022
 *   Denis McLaughlin
 *   - got marching pixel to work, with direction change on click and row
 *     increment and decrement on rotate
 *
 *  Tuesday, April 5, 2022
 *   Denis McLaughlin
 *   - added paintPaddle() routine, and arrays to support mapping paddle
 *     position to the relevant LEDs
 */

#include <EEPROM.h>

const char *project_info = "\nspp\nMarch 14, 2021\nDenis McLaughlin\n";

#define NUM_LEDS 64      // Number of LED's.
uint8_t max_bright = 40; // Overall brightness definition. It can be changed on the fly.

// Comment out if using button instead of rotary encoder
#define USING_ENCODER true;

#ifdef ESP32
#define buttonPin 17 // Digital pin connected sensor
#define LED_DT 16    // Data pin to connect to the strip. 4:ESP8266, 16:ESP32
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

//Mode and EEPROM variables Maximum number of display modes. Would prefer to
//get this another way, but whatever.
uint8_t maxMode = 6; int eepaddress = 0;

// Global variables can be changed on the fly.
struct CRGB leds[NUM_LEDS]; // Initialize our LED array.

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns
uint8_t gIntensity[] = {10, 30, 60, 80, 100};
uint8_t gCurrentIntensity = 1;

// available modes of operation of the gadget
enum mode
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

  // A safety in case the EEPROM has an illegal value.
  if (gCurrentPatternNumber > maxMode)
    gCurrentPatternNumber = 0;
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

// variables for spp
int led=0;
int increment=1;
int ppos=0;
int plen=6;

// this array is to map a paddle position [0:28] to corresponding LED positions
uint8_t paddleLEDs[] = {
00,01,02,03,04,05,06,07,
15,23,31,39,47,55,63,
62,61,60,59,58,57,56,
48,40,32,24,16,8
};

void loop()
{
    leds[led] = CRGB::Green; FastLED.show(); delay(30); 
    leds[led] = CRGB::Black; FastLED.show(); delay(30);

    led += increment;
    // if ( led > NUM_LEDS-1 ) { led=0; }
    // if ( led < 0 )          { led=NUM_LEDS-1; }

    readbutton_encoder();
    doBBEncoder();

    if ( encoder0Pos > oldEncoder0Pos )
       led+=8;
    if ( encoder0Pos < oldEncoder0Pos )
       led-=8;
    oldEncoder0Pos = encoder0Pos;

    // make sure the led we're changing stays in range
    led %= NUM_LEDS;

    paintPaddle(0);
    ppos += increment;
    if ( ppos > 27 ) { ppos=0; } 
    if ( ppos < 0 )  { ppos=27; } 
    paintPaddle(1);
}

// This routine uses the global ppos (paddle position) and plen (paddle length)
// variables to compute which LEDs should be activated for the paddle: if
// paint is passed as non-zero, it will paint the paddle in its position, if
// it's 0, then it paints it black
void paintPaddle(int paint)
{
    int color=CRGB::Green;
    if ( !paint ) { color=CRGB::Black; }

    int start=ppos;
    for (int i=0; i<plen; i++)
    {
        int index=(ppos+i)%28;
        int led=paddleLEDs[index];
        leds[led] = color;
    }

    FastLED.show();
}

void readbutton_encoder()
{ // Read the button and increase the mode

  uint8_t b = checkButton();

  // single click
  if (b == 1)
  {
    // if we're sleeping, wake up
    if (SLEEPING)
    {
      SLEEPING = false;
      led = 0;
      increment = 1;
    }
    // otherwise reverse the direction of the moving led
    else
    {
        increment *= -1;
    }
  }

  // double click
  if (b == 2)
  {
  }

  // long press: sleep the device
  if (b == 3)
  {
    SLEEPING = true;

    fadeToBlackBy(leds, NUM_LEDS, 50);
    delay(50);
    FastLED.show();
    fadeToBlackBy(leds, NUM_LEDS, 50);
    delay(50);
    FastLED.show();
    FastLED.clear();
    FastLED.show();
  }

}


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
