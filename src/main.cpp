/*****************************************************************************

  EANx Analysis with output to an OLED color display

  Reads an analog input from an ADC, converts it to mV, and creates a running average
  of ADC values and and prints the result to the display and debug to Serial Monitor.

  Based on prior EANx scripts:
  https://github.com/ppppaoppp/DIY-Nitrox-Analyzer-04_12_2019.gitf
  https://github.com/ejlabs/arduino-nitrox-analyzer.git

  Brian Ehrler 2022-2023
*****************************************************************************/

// Libraries
#include <Arduino.h>
#include <RunningAverage.h>
#include <SPI.h>
#include <Adafruit_GFX.h> // Core graphics library
#include <TFT_eSPI.h>
#include <Adafruit_ADS1X15.h>
#include "pin_config.h"
#include "version.h"

// Debugging
#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

// Display Definitions
#define TFT_WIDTH 240  // OLED display width, in pixels
#define TFT_HEIGHT 240 // OLED display height, in pixels
#define ResFact 2      // 1 = 128x128   2 = 240x240 
#define ROUNDISP 0     // 1= yes 0= no

// User Interface Settings -----------------------------------------------------------------
#define GUI 0       // 1= on 0= off
#define nerds 1     // 1= on 0= off   GUI of 1 will override this setting
#define metric 0    // 1= on 0= off   Available since 1866 in the US
#define statinfo 1  // 2= bottom 1= top, 0= off [GUI requires off]
#define MOD 1       // 1= on 0= off
#define RODA 0      // 1= on 0= off


// Init tft and sprites
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite gauge = TFT_eSprite(&tft);
TFT_eSprite background = TFT_eSprite(&tft);
TFT_eSprite needle = TFT_eSprite(&tft);
  
// Init ADS
Adafruit_ADS1115 ads; // Define ADC - 16-bit version

// Running Average definitions
#define RA_SIZE 20          // Define running average pool size
RunningAverage RA(RA_SIZE); // Initialize Running Average

// Global Variables
int LCDROT = 0; // 0 = default, 1 = CW 90
float tbFactor = 0;
float spFactor = 0;
float prevaveSensorValue = 0;
float aveSensorValue = 0;
float mVolts = 0;
int ADSchk = 0;
float batVolts = 0;
float prevO2 = 0;
float currentO2 = 0;
float calFactor = 1;
float calErrChk = 1;
int mod14fsw = 0;
int mod14msw = 0;
int mod16fsw = 0;
int mod16msw = 0;
float modppo = 1.4;
float mod16ppo = 1.6;
float multiplier = 0;
int msgid = 0;

#if GUI == 1
// Define display colors
#define backColor TFT_BLACK
#define gaugeColor 0x055D
#define dataColor 0x0311
#define needleColor 0xF811
#define color1 TFT_WHITE
#define color2 0x8410
#define color3 TFT_ORANGE
#define color4 0x15B3
#define color5 0x00A3

#define DEG2RAD 0.0174532925

// Gauge variables
float o2Angle = 0;  // range 0-100
float modAngle = 0; // range 10-250

int centerX = 120; // centerX and sy
int centerY = -75;
int centerX2 = 120;
int centerY2 = 330;
int r = 180; // outer radius tickmarks ... offset r-5 for line end, r-15 for text

int uprAngle = 0;
int lwrAngle = 0;
int o2OffAgl = 90;
int modOffAgl = 270;

String o2[10] = {"0", "10", "20", "30", "40", "50", "60", "70", "80", "90"};
String mod[10] = {"0", "30", "60", "90", "120", "150", "180", "210", "240", "270"};
int o2Start[10];
int o2StartP[60];
int modStart[10];
int modStartP[60];

float x[360]; // pixel points O2 gauge
float y[360];
float px[360]; // line end points O2 gauge
float py[360];
float lx[360]; // text of O2 gauges
float ly[360];

float x2[360]; // outer points of MOD gauges
float y2[360];
float px2[360]; // ineer point of MOD gauges
float py2[360];
float lx2[360]; // text of MOD gauges
float ly2[360];

#endif

const int buttonPin = BUTTON_PIN; // push button

// Functions
float batStat();

/*void gotoSleep() 
{
  Serial.println("going to sleep in 3 sec....");   
  delay(3000); 
  esp_deep_sleep_enable_gpio_wakeup(1ULL << buttonPin,ESP_GPIO_WAKEUP_GPIO_HIGH);  
  digitalWrite(TFT_BL, LOW); // turns off backlight
  delay(120); // Delay for shutdown time before another command can be sent
  Serial.println("going to sleep now"); 
  delay(1000);
  tft.writecommand(0x10); // Sleep
  delay(5); // Delay for shutdown time before another command can be sent
  esp_deep_sleep_start();
  //esp_light_sleep_start();
}*/

void SenseFault()
{
  debugln("Low mV reading from Sensor");
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1 * ResFact);
  tft.drawCentreString("Error", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 4);
  tft.drawCentreString("Replace", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.4, 4);
  tft.drawCentreString("O2 Sensor", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.7, 4);
  tft.setTextSize(1);
  tft.drawCentreString(String(mVolts), TFT_WIDTH * .5, TFT_HEIGHT * 0.9, 4);
  delay(30000);
}

void BattFault()
{
  debugln("Low V reading from Battery");
  tft.fillScreen(TFT_YELLOW);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(1 * ResFact);
  tft.drawCentreString("Error", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 4);
  tft.drawCentreString("Recharge", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.4, 4);
  tft.drawCentreString("Battery", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.7, 4);
  tft.setTextSize(1);
  tft.drawCentreString(String(batVolts), TFT_WIDTH * .5, TFT_HEIGHT * 0.9, 4);
  delay(30000);
}

float initADC()
{
  // init ADC and Set gain

  // The ADC input range (or gain) can be changed via the following
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  ads.setGain(GAIN_TWO); // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

  /* Be sure to update this value based on the IC and the gain settings! */
  // float   multiplier = 3.0F;    /* ADS1015 @ +/- 6.144V gain (12-bit results) */
  float multiplier = 0.0625; /* ADS1115  @ +/- 6.144V gain (16-bit results) */

  // Check that the ADC is operational
  if (!ads.begin())
  {
    debugln("Failed to initialize ADC.");
    tft.fillScreen(TFT_YELLOW);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1 * ResFact);
    tft.drawCentreString("Error", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 4);
    tft.drawCentreString("ADC", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.4, 4);
    tft.drawCentreString("Failure", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.7, 4);
    delay(30000);
    while (1);
  }
  ADSchk = 1;
  return (multiplier);
}

void o2calibration()
{
  int cal = 1;
  do
  {
    // display "Calibrating"
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1 * ResFact);
    tft.drawCentreString("+++++++++++++", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
    tft.drawCentreString("Calibrating", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.3, 2);
    tft.drawCentreString("O2 Sensor", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.6, 2);
    tft.drawCentreString("+++++++++++++", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.8, 2);
    debugln("Calibration Screen Text");

    // get running average value from ADC input Pin
    RA.clear(); 
    for (int x = 0; x <= (RA_SIZE * 3); x++)
    {
      int sensorValue = 0;
      sensorValue = abs(ads.readADC_Differential_0_1());
      RA.addValue(sensorValue);
      delay(12); // was 8
    }
    debug("Average calibration read ");
    debugln(RA.getAverage()); // average cal factor serial print for debugging
    calFactor = RA.getAverage();
    delay(1000); // Slow the loop for checksum

    // Checksum on calibrate ... is the sensor still reseting from earlier read
    RA.clear();
    for (int x = 0; x <= (RA_SIZE * 3); x++)
    {
      int sensorValue = 0;
      sensorValue = abs(ads.readADC_Differential_0_1());
      RA.addValue(sensorValue);
      delay(36); // was 24
    }

    tft.fillScreen(TFT_BLACK);

    calErrChk = RA.getAverage();
    debug("calibration raw values Calfactor=");
    debug(calFactor); // average cal factor
    debug(" CalErrChk=");
    debugln(calErrChk); // average cal err factor serial print for debugging

    debugln(abs((calFactor / calErrChk) - 1) * 100);

    // checking against a 0.5% error
    if (abs((calFactor / calErrChk) - 1) * 100 > 0.5)
    {
      debugln("Calibration Checksum out of spec");
      tft.fillScreen(TFT_ORANGE);
      tft.setTextColor(TFT_BLACK);
      tft.setTextSize(1 * ResFact);
      tft.drawCentreString("+++++++++++++", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
      tft.drawCentreString("Refining", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.25, 2);
      tft.drawCentreString("Calibration", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.45, 2);
      tft.drawCentreString("Standby", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.65, 2);
      tft.drawCentreString("+++++++++++++", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.80, 2);
      tft.drawCentreString(String(abs((calFactor / calErrChk) - 1) * 100), TFT_WIDTH * 0.5, TFT_HEIGHT * 0.80, 1);
      delay(2000);
      cal = 1;
    }
    else
    {
      cal = 0;
    }
  } while (cal);

  calFactor = (1 / RA.getAverage() * 20.900); // Auto Calibrate to 20.9%
}

void BatGauge(int locX, int locY, float batV)
{

#if GUI == 1
  // Add to gauge sprite
  gauge.drawRect(locX, locY, 25, 12, TFT_WHITE);
  gauge.drawRect((locX + 25), (locY + 4), 3, 4, TFT_WHITE);
  gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_BLACK);

  // Fill with the color that matches the charge state
  if (batV >= 3.4 and batV < 3.6)
  {
    gauge.fillRect((locX + 1), (locY + 1), 15, 10, TFT_YELLOW);
  }
  if (batV < 3.4)
  {
    gauge.fillRect((locX + 1), (locY + 1), 10, 10, TFT_RED);
  }
  if (batV >= 3.6)
  {
    gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_GREEN);
  }
#else
  // Draw the outline and clear the box
  tft.drawRect(locX, locY, 25, 12, TFT_WHITE);
  tft.drawRect((locX + 25), (locY + 4), 3, 4, TFT_WHITE);
  tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_BLACK);

  // Fill with the color that matches the charge state
  if (batV > 3.4 and batV < 3.6)
  {
    tft.fillRect((locX + 1), (locY + 1), 15, 10, TFT_YELLOW);
  }
  if (batV < 3.4)
  {
    tft.fillRect((locX + 1), (locY + 1), 10, 10, TFT_RED);
  }
  if (batV > 3.6)
  {
    tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_GREEN);
  }
#endif
  if (batV < 3.2) { BattFault(); }

}

void SenseGauge(int locX, int locY, float senV)
{

#if GUI == 1
  // Draw the outline and clear the box
  gauge.drawRect(locX, locY, 25, 12, TFT_WHITE);
  gauge.drawRect((locX + 5), (locY - 3), 4, 3, TFT_WHITE);
  gauge.drawRect((locX + 16), (locY - 3), 4, 3, TFT_WHITE);
  gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_BLACK);

  // Fill with the color that matches the charge state
  if (senV >= 9.0)
  {
    gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_BLUE);
  }
  if (senV >= 8 and senV < 9.0 )
  {
    gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_GREEN);
  }
  if (senV > 7.5 and senV < 8.0)
  {
    gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_YELLOW);
  }
  if (senV <= 7.5)
  {
    gauge.fillRect((locX + 1), (locY + 1), 23, 10, TFT_RED);
  }

#else
  // Draw the outline and clear the box
  tft.drawRect(locX, locY, 25, 12, TFT_WHITE);
  tft.drawRect((locX + 5), (locY - 3), 4, 3, TFT_WHITE);
  tft.drawRect((locX + 16), (locY - 3), 4, 3, TFT_WHITE);
  tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_BLACK);

  // Fill with the color that matches the charge state
  if (senV > 7.5 and senV < 8.0)
  {
    tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_YELLOW);
  }
  if (senV >= 8 and senV < 9.0 )
  {
    tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_GREEN);
  }
  if (senV <= 7.5)
  {
    tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_RED);
  }
  if (senV >= 9.0)
  {
    tft.fillRect((locX + 1), (locY + 1), 23, 10, TFT_BLUE);
  }
#endif

if (senV < 7.1) { SenseFault(); }

}

void testfillcircles(uint8_t radius, uint16_t color)
{
  for (int16_t x = radius; x < tft.width(); x += radius * 2)
  {
    for (int16_t y = radius; y < tft.height(); y += radius * 2)
    {
      tft.fillCircle(x, y, radius, color);
    }
  }
}

void testdrawcircles(uint8_t radius, uint16_t color)
{
  for (int16_t x = 0; x < tft.width() + radius; x += radius * 2)
  {
    for (int16_t y = 0; y < tft.height() + radius; y += radius * 2)
    {
      tft.drawCircle(x, y, radius, color);
    }
  }
}

void safetyrule()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1 * ResFact);
  randomSeed(millis());
  int randNumber = random(5);
  if (randNumber == 0)
  {
    tft.drawCentreString("Seek proper", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
    tft.drawCentreString("training", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.25, 2);
  }
  else if (randNumber == 1)
  {
    tft.drawCentreString("Maintain a", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
    tft.drawCentreString("continious", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.25, 2);
    tft.drawCentreString("guideline to", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.4, 2);
    tft.drawCentreString("the surface", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.55, 2);
  }
  else if (randNumber == 2)
  {
    tft.drawCentreString("Stay within", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
    tft.drawCentreString("your depth", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.25, 2);
    tft.drawCentreString("limitations", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.4, 2);
  }
  else if (randNumber == 3)
  {
    tft.drawCentreString("Proper gas", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
    tft.drawCentreString("management", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.25, 2);
  }
  else
  {
    tft.drawCentreString("Use appropriate", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 2);
    tft.drawCentreString("properly maintaned", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.25, 2);
    tft.drawCentreString("equipment", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.4, 2);
  }
  delay(2000);
  tft.fillScreen(TFT_BLACK);
}

void displayUtilData()
{

  BatGauge((TFT_WIDTH * 0.8), (TFT_HEIGHT * (tbFactor + 0.02)), (batVolts));
  SenseGauge((TFT_WIDTH * 0.1), (TFT_HEIGHT * (tbFactor + 0.02)), (mVolts));

  // Stats for nerds text
  #if (nerds == 1 and GUI == 0)

    tft.setTextSize(1);
    if (mVolts > 7.5 and mVolts <= 9.0) { tft.setTextColor(TFT_YELLOW, TFT_BLACK); }
    if (mVolts <= 7.5) { tft.setTextColor(TFT_RED, TFT_BLACK); }
    if (mVolts > 9.0) { tft.setTextColor(TFT_SKYBLUE, TFT_BLACK); }
    if (mVolts < 7.1) { SenseFault(); }
    String mv = String(mVolts, 1);

    tft.drawCentreString(String(mv + " mV "), TFT_WIDTH * 0.18, TFT_HEIGHT * 0.1, 2);

    if (batVolts > 3.4 and batVolts < 3.6) { tft.setTextColor(TFT_YELLOW, TFT_BLACK); }
    if (batVolts < 3.4) { tft.setTextColor(TFT_RED, TFT_BLACK); }
    if (batVolts > 3.6) { tft.setTextColor(TFT_GREEN, TFT_BLACK); }

    String bv = String(batVolts, 1);
    if (batVolts < 3.2) { BattFault(); }

    tft.drawCentreString(String(bv + " V  "), TFT_WIDTH * 0.88, TFT_HEIGHT * 0.1, 2);

    //tft.drawString(String(millis() / 1000), TFT_WIDTH * 0.05, TFT_HEIGHT * 0, 2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString(String(VERSION), TFT_WIDTH * 0.5, TFT_HEIGHT * 0.93, 2);
  #endif
}

#if GUI == 0
void textBaseLayout()
{
  // Draw Layout -- Adjust this layouts to suit you LCD

  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.setTextSize(1 * ResFact);
  tft.drawCentreString("O %", TFT_WIDTH * 0.5, TFT_HEIGHT * 0, 4);
  tft.setTextSize(1);
  tft.drawCentreString("2", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.1, 4);
  tft.setTextSize(1 * ResFact);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  if (MOD == 1)
  {
    tft.drawCentreString("@1.4  MOD  @1.6", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.62, 2);
  }
  
  debugln("Base layout");
}

void displayTextData()
{
  // Generate Text layout
  if (prevO2 != currentO2)
  {
    if (currentO2 > 19.5 and currentO2 < 22)
    {
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
    }
    if (currentO2 <= 19.5)
    {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    }
    if (currentO2 <= 17)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    if (currentO2 >= 22)
    {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }

    // Draw Text Layout -- Adjust these layouts to suit you LCD
    tft.setTextSize(1 * ResFact);
    String o2 = String(currentO2, 1);
    float h = 0.22;
    if (MOD == 0)
    {
      h = 0.35;
    }

    tft.drawCentreString(o2, TFT_WIDTH * 0.5, TFT_HEIGHT * h, 7);
    debugln("Write O2 string");
  
    tft.setTextSize(1 * ResFact);
    if (MOD == 1)
    {
      if (metric == 1)
      {
        String mod14m = String(mod14msw);
        tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
        tft.drawString(String(mod14m + "-m  "), TFT_WIDTH * 0.05, TFT_HEIGHT * 0.75, 2);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);
        String mod16m = String(mod16msw);
        tft.drawString(String(mod16m + "-m  "), TFT_WIDTH * 0.7, TFT_HEIGHT * 0.75, 2);
      }
      else
      {
        tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
        String mod14f = String(mod14fsw);
        tft.drawString(String(mod14f + "-FT  "), TFT_WIDTH * 0, TFT_HEIGHT * 0.75, 2);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);
        String mod16f = String(mod16fsw);
        tft.drawString(String(mod16f + "-FT  "), TFT_WIDTH * 0.6, TFT_HEIGHT * 0.75, 2);
      }
    }
  }
}
#endif

#if GUI == 1
void gaugeBaseLayout()
{
  // Draw Layout -- Adjust this layouts to suit you LCD
  gauge.createSprite(TFT_WIDTH, 240);
  gauge.setSwapBytes(true);
  gauge.setTextDatum(4);
  gauge.setTextColor(TFT_WHITE, backColor);
  needle.createSprite(TFT_WIDTH, 240);
  needle.setSwapBytes(true);
  background.createSprite(TFT_WIDTH, 240);
  background.setSwapBytes(true);
  background.fillSprite(TFT_BLACK);

  int b = 0;
  int b2 = 0;
  int c = 0;
  int c2 = 0;

  // Set up polar coordinates for gauge
  for (int i = 0; i < 360; i++)
  {
    x[i] = ((r)*cos(DEG2RAD * i)) + centerX; // Shading
    y[i] = ((r)*sin(DEG2RAD * i)) + centerY;
    px[i] = ((r - 14) * cos(DEG2RAD * i)) + centerX; // pick marks
    py[i] = ((r - 14) * sin(DEG2RAD * i)) + centerY;
    lx[i] = ((r - 24) * cos(DEG2RAD * i)) + centerX; // line ends
    ly[i] = ((r - 24) * sin(DEG2RAD * i)) + centerY;

    if (i % 36 == 0)
    {
      o2Start[b] = i;
      b++;
    }

    if (i % 6 == 0)
    {
      o2StartP[b2] = i;
      b2++;
    }

    x2[i] = ((r)*cos(DEG2RAD * i)) + centerX2; // Shading
    y2[i] = ((r)*sin(DEG2RAD * i)) + centerY2;
    px2[i] = ((r - 14) * cos(DEG2RAD * i)) + centerX2; // pick marks
    py2[i] = ((r - 14) * sin(DEG2RAD * i)) + centerY2;
    lx2[i] = ((r - 24) * cos(DEG2RAD * i)) + centerX2; // line ends
    ly2[i] = ((r - 24) * sin(DEG2RAD * i)) + centerY2;

    if (i % 36 == 0)
    {
      modStart[c] = i;
      c++;
    }

    if (i % 6 == 0)
    {
      modStartP[c2] = i;
      c2++;
    }
  }

  debugln("Gauges Create");
}

void displayGaugeData()
{

  // Text info
  if (currentO2 != prevO2)
  {
    if (currentO2 > 20 and currentO2 < 22)
    {
      gauge.setTextColor(TFT_CYAN, color5);
    }
    if (currentO2 <= 20)
    {
      gauge.setTextColor(TFT_YELLOW, color5);
    }
    if (currentO2 <= 18)
    {
      gauge.setTextColor(TFT_RED, color5);
    }
    if (currentO2 >= 22)
    {
      gauge.setTextColor(TFT_GREEN, color5);
    }

    /*
        tft.setTextSize(1 * ResFact);

        if (metric == 1)  {
          String mod14m = String(mod14msw);
          tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
          tft.drawString(String(mod14m + "-m  "), TFT_WIDTH * 0.05, TFT_HEIGHT * 0.75, 2);
          tft.setTextColor(TFT_GOLD, TFT_BLACK);
          String mod16m = String(mod16msw);
          tft.drawString(String(mod16m + "-m  "), TFT_WIDTH * 0.7, TFT_HEIGHT * 0.75, 2);
        }
        else {
          tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
          String mod14f = String(mod14fsw);
          tft.drawString(String(mod14f + "-FT  "), TFT_WIDTH * 0, TFT_HEIGHT * 0.75, 2);
          tft.setTextColor(TFT_GOLD, TFT_BLACK);
          String mod16f = String(mod16fsw);
          tft.drawString(String(mod16f + "-FT  "), TFT_WIDTH * 0.6, TFT_HEIGHT * 0.75, 2);
        }
    */
    // Draw gauge with needle
    gauge.fillSprite(TFT_BLACK);

    // Upper Gauge
    gauge.fillCircle(centerX, centerY, r + 20, TFT_DARKCYAN);
    gauge.fillCircle(centerX, centerY, r - 30, color5);
    gauge.setTextSize(1);

    // Lower Gauge
    gauge.fillCircle(centerX2, centerY2, r + 20, TFT_DARKGREEN);
    gauge.fillCircle(centerX2, centerY2, r - 30, color5);
    gauge.setTextSize(1);

    // Upper Gauge value
    o2Angle = (360 - ((currentO2)*3.6));

    uprAngle = nearbyint(o2Angle) + o2OffAgl;

    if (uprAngle >= 360)
      o2OffAgl = o2OffAgl - 360;

    if (uprAngle < 0)
      o2OffAgl = o2OffAgl + 360;

    String ox = String(currentO2, 1);
    gauge.drawCentreString(ox, centerX, centerY + 90, 6);
    // debugln(uprAngle);
    // debugln(o2Angle);

    // Lower Gauge value
    if (currentO2 > 14)
    {
      modAngle = (360 - ((mod14fsw)*1.2));
      String modx = String(mod14fsw);
      String modc = String(mod16fsw);
      gauge.setTextColor(TFT_GREENYELLOW, color5);
      gauge.drawCentreString(modx, centerX2, centerY2 - 140, 6);
      gauge.setTextColor(TFT_ORANGE, TFT_BLACK);
      gauge.setTextSize(2);
      gauge.drawCentreString(modc, centerX2 + 95, centerY2 - 217, 2);
    }

    lwrAngle = nearbyint(modAngle) + modOffAgl;

    if (lwrAngle >= 360)
      modOffAgl = modOffAgl - 360;

    if (lwrAngle < 0)
      modOffAgl = modOffAgl + 360;

    // debugln(lwrAngle);
    // debugln(modAngle);

    // Draw Upper Gauge Markings
    gauge.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    gauge.setTextSize(2);
    gauge.drawWedgeLine(centerX, centerY + 170, centerX, centerY + 155, 1, 6, TFT_ORANGE);

    for (int i = 0; i < 10; i++)
    {
      if (o2Start[i] + uprAngle < 360)
      {
        gauge.drawString(o2[i], x[o2Start[i] + uprAngle], y[o2Start[i] + uprAngle]);
        gauge.drawLine(px[o2Start[i] + uprAngle], py[o2Start[i] + uprAngle], lx[o2Start[i] + uprAngle], ly[o2Start[i] + uprAngle], color1);
      }
      else
      {
        gauge.drawString(o2[i], x[(o2Start[i] + uprAngle) - 360], y[(o2Start[i] + uprAngle) - 360]);
        gauge.drawLine(px[(o2Start[i] + uprAngle) - 360], py[(o2Start[i] + uprAngle) - 360], lx[(o2Start[i] + uprAngle) - 360], ly[(o2Start[i] + uprAngle) - 360], color1);
      }
    }

    for (int i = 0; i < 60; i++)
    {
      if (o2StartP[i] + uprAngle < 360)
      {
        gauge.fillCircle(px[o2StartP[i] + uprAngle], py[o2StartP[i] + uprAngle], 1, color3);
      }
      else
      {
        gauge.fillCircle(px[(o2StartP[i] + uprAngle) - 360], py[(o2StartP[i] + uprAngle) - 360], 1, color3);
      }
    }

    // Draw Lower Gauge Markings
    gauge.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    gauge.setTextSize(2);
    gauge.drawWedgeLine(centerX2, centerY2 - 170, centerX2, centerY2 - 155, 1, 6, TFT_ORANGE);

    for (int i = 0; i < 10; i++)
    {
      if (modStart[i] + lwrAngle < 360)
      {
        gauge.drawString(mod[i], x2[modStart[i] + lwrAngle], y2[modStart[i] + lwrAngle]);
        gauge.drawLine(px2[modStart[i] + lwrAngle], py2[modStart[i] + lwrAngle], lx2[modStart[i] + lwrAngle], ly2[modStart[i] + lwrAngle], color1);
      }
      else
      {
        gauge.drawString(mod[i], x2[(modStart[i] + lwrAngle) - 360], y2[(modStart[i] + lwrAngle) - 360]);
        gauge.drawLine(px2[(modStart[i] + lwrAngle) - 360], py2[(modStart[i] + lwrAngle) - 360], lx2[(modStart[i] + lwrAngle) - 360], ly2[(modStart[i] + lwrAngle) - 360], color1);
      }
    }

    for (int i = 0; i < 60; i++)
    {
      if (modStartP[i] + lwrAngle < 360)
      {
        gauge.fillCircle(px2[modStartP[i] + lwrAngle], py2[modStartP[i] + lwrAngle], 1, color3);
      }
      else
      {
        gauge.fillCircle(px2[(modStartP[i] + lwrAngle) - 360], py2[(modStartP[i] + lwrAngle) - 360], 1, color3);
      }
    }

#if statinfo != 0
    displayUtilData();
#endif

    gauge.pushSprite(0, spFactor);
  }

}
#endif

void setup()
{

  Serial.begin(115200);
  // Call our validation to output the message (could be to screen / web page etc)
  printVersionToSerial();

  pinMode(buttonPin, INPUT);
  debugln("Pinmode Init");

  // setup TFT
  tft.init();
  debugln("TFT Init");
  
  // tft.invertDisplay(1);
  tft.setRotation(LCDROT);
  tft.fillScreen(TFT_BLACK);

  debugln("Display Initialized");

  tft.fillScreen(TFT_BLACK);
  testfillcircles(5, TFT_BLUE);
  testdrawcircles(5, TFT_WHITE);
  delay(500);


  // setup display and calibrate unit
  initADC();
  debugln("Post ADS check statement");

  // Unit Information Screen
  tft.fillScreen(TFT_DARKCYAN);
  // tft.setTextSize(1 * ResFact);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  if (ADSchk == 1) 
  { 
    tft.setTextColor(TFT_GREEN);
    tft.drawCentreString("ADC Good", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.15, 4);
  }
  else 
  {
    tft.setTextColor(TFT_RED);
    tft.drawCentreString("ADC Issue", TFT_WIDTH * 0.5, TFT_HEIGHT * 0.15, 4);
  }
  // String mVtest = String(mVolts);  
  float mVtestval = ((abs(ads.readADC_Differential_0_1())) * .0625);
  String mVtest = String (mVtestval);
 if (mVtestval <= 8.0 )
 {
  tft.setTextColor(TFT_RED);
  tft.drawCentreString(String("Sensor mV " + mVtest), TFT_WIDTH * 0.5, TFT_HEIGHT * 0.3, 4);
 }
 else
 {
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString(String("Sensor mV " + mVtest), TFT_WIDTH * 0.5, TFT_HEIGHT * 0.3, 4);
 } 
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(MODEL, TFT_WIDTH * 0.5, TFT_HEIGHT * 0.45, 4);
  tft.drawCentreString(VERSION, TFT_WIDTH * 0.5, TFT_HEIGHT * 0.6, 4);
#if ESP32
  tft.drawCentreString(String (chipId), TFT_WIDTH * 0.5, TFT_HEIGHT * 0.75, 4);
#endif

  delay(5000);

  tft.fillScreen(TFT_BLACK);

  o2calibration();
  debugln("Post O2 calibration");

#if RODA == 1
  safetyrule();
#endif

#if statinfo == 2
  tbFactor = 0.65;
  spFactor = 0;
#endif

#if GUI == 1
  gaugeBaseLayout(); // Graphic Layout
#else
  debugln("PRE Text Based layout");
  textBaseLayout();  // Text Layout
  debugln("POST Text Based layout");
#endif

  debugln("Setup Complete Starting Loop");

}

// the loop routine runs over and over again forever:
void loop()
{

  multiplier = initADC();
  /* debugln("Button Check");

  int bstate = digitalRead(buttonPin);
  //debugln(bstate);
  if (bstate == LOW) // must be LOW for TTGO OI
  {
    //gotoSleep();

    if (LCDROT == 0)
    {
      LCDROT = 2;
      //digitalWrite(TFT_BL, LOW); // turns off backlight

    }
    else
    {
      LCDROT = 0;
      //digitalWrite(TFT_BL, HIGH); // turns off backlight
    }
    tft.setRotation(LCDROT);
    tft.fillScreen(TFT_BLACK);
  }

*/
  // get running average value from ADC input Pin
  RA.clear();
  for (int x = 0; x <= RA_SIZE; x++)
  {
    int sensorValue = 0;
    sensorValue = abs(ads.readADC_Differential_0_1());
    RA.addValue(sensorValue);
    delay(8); // was 16
    // debugln(sensorValue);    //mV serial print for debugging
  }

  delay(100); // slowing down loop a bit

  // Record old and new ADC values
  prevaveSensorValue = aveSensorValue;
  prevO2 = currentO2;
  aveSensorValue = RA.getAverage();

  currentO2 = (aveSensorValue * calFactor); // Units: pct

  // currentO2 = currentO2 + 5; // Test Values

  if (currentO2 > 99.9)
    currentO2 = 99.9;

  mVolts = (aveSensorValue * multiplier); // Units: mV

#ifdef ESP32
    batVolts = (batStat() / 1000) * BAT_ADJ; // Battery Check ESP based boards
#endif

  mod14fsw = floor(33 * ((modppo / (currentO2 / 100)) - 1));
  mod14msw = floor(10 * ((modppo / (currentO2 / 100)) - 1));
  mod16fsw = floor(33 * ((mod16ppo / (currentO2 / 100)) - 1));
  mod16msw = floor(10 * ((mod16ppo / (currentO2 / 100)) - 1));

  // DEBUG print out the value you read:
  msgid++;
  debug("Msg_ID:");
  debug(msgid);
  debug("\t");
  debug("ADC:");
  debug(aveSensorValue);
  debug("\t");
  debug("Sensor_mV:");
  debug(mVolts);
  debug("\t");
  debug("Batt_V:");
  debug(batVolts);
  debug("\t");
  debug("O2:");
  debug(currentO2);
  debug("\t");
  debug("MOD 1.4:");
  debug(mod14fsw);
  debug("\t");
  debug("MOD 1.6:");
  debugln(mod16fsw);

#if statinfo != 0
  textBaseLayout();
  displayUtilData();
#endif

#if GUI == 1
  displayGaugeData(); // Graphic Layout
#else
  displayTextData(); // Text Layout
#endif
}