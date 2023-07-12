#define VERSION "as1_dimmer_9"
// NO Temperature Correction
// 16-bit !!!
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // include i/o class header
#include <CommonBusEncoders.h>

#include <OneWire.h> 
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 12 

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature dtemp(&oneWire);

// declare the lcd object
hd44780_I2Cexp lcd; // auto locate and autoconfig interface pins

// tell the hd44780 sketch the lcd object has been declared
#define HD44780_LCDOBJECT

#define LCD_COLS 20
#define LCD_ROWS 4

CommonBusEncoders encoders(A0, A1, A2, 3);

uint32_t enc1;
uint32_t enc2;
uint32_t enc3;
//const uint16_t maxValue =  1023; //10-bit resolution
const uint32_t maxValue =  65535; //16-bit resolution

uint32_t stepTiny     =    1;
uint32_t stepSmall    =    4;
uint32_t stepBig      =   16;
uint32_t stepExtraBig =  256;
uint32_t stepHuge     = 1024;


struct colorStruct {
  int pin;
  char name[6];
  uint32_t encValue;
//  uint16_t value;
  int step;
};

#define NUM_COLORS 4

struct colorStruct color[NUM_COLORS] = {
//{ 9, "Red  ", 0, 0, stepExtraBig},
//{10, "Green", 0, 0, stepExtraBig},
//{ 5, "Blue ", 0, 0, stepExtraBig},
//{11, "Amber", 0, 0, stepExtraBig},
{ 9, "Red  ", 0, stepExtraBig},
{10, "Green", 0, stepExtraBig},
{ 5, "Blue ", 0, stepExtraBig},
{11, "Amber", 0, stepExtraBig},
};

int curColorIndex = 0;
struct colorStruct curColor = color[curColorIndex];

float fTemp;

/* Configure digital pins 9, 10, 11, 5 as 10-bit PWM outputs. */
// for Leonardo
void setupPWM10() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
        | _BV(WGM11) | _BV(WGM10);                 /* mode 3: Phase correct, 10-bit */

  TCCR1B = _BV(CS11); //prescaler=8 Freq = 976Hz

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1)                             /* non-inverting PWM */
        | _BV(WGM31) | _BV(WGM30);                 /* mode 3: Phase correct, 10-bit */
  TCCR3B = _BV(CS31); //prescaler=8 Freq = 976Hz
}
// Configure digital pins 9, 10, 11 and 5 as 16-bit PWM outputs.
// for Leonardo
void setupPWM16() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
        | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); //prescaler=1
  ICR1 = 0xFFFF;

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1) /* non-inverting PWM */
        | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR3B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); //prescaler=1
  ICR3 = 0xFFFF;
}
/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11, 5 */
void analogWriteHR(uint8_t pin, uint16_t val) {
  switch (pin) {
    case  9: OCR1A = val; break; //Red
    case 10: OCR1B = val; break; //Green
    case 11: OCR1C = val; break; //Amber
    case  5: OCR3A = val; break; //Blue
  }
}
//####################################################################
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String(VERSION));
  delay(5000);
  lcd.clear();

  dtemp.begin();
  dtemp.setWaitForConversion(false); //non-blocking
  dtemp.requestTemperatures();

  encoders.setDebounce(16);
  encoders.addEncoder(1, 4, A3, 1, 100,   1);
  encoders.addEncoder(2, 4, A4, 1, 200,   2);
  encoders.addEncoder(3, 4, A5, 1, 300,   3);

  setupPWM16();
  updateDisplay();
}
//######################################
String toStr4(int i) {
  String result = "";
  if (i < 10) result += "0";
  if (i < 100) result += "0";
  if (i < 1000) result += "0";
  result += i;
  return result;
}
//######################################
String toStrN(uint16_t i, int N) {
  String result = "";
  for (int pos=1; pos < N; pos++) {
    if (i < pow(10,pos)) result += "0";
  }
  result += i;
  return result;
}
//####################################################################
void setBrightness() {
  for (int i = 0; i < NUM_COLORS; i++) {
    uint16_t val;
    val = color[i].encValue;
//    if (val > maxValue) val = maxValue;
//    color[i].value = val;
    analogWriteHR(color[i].pin, val);
  }
}
//####################################################################
void updateDisplay() {
  setBrightness();
//  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String("R=") + toStrN(color[0].encValue, 5) + String("(") + toStrN(color[0].step, 4) + String(")"));
  lcd.setCursor(0, 1);
  lcd.print(String("G=") + toStrN(color[1].encValue, 5) + String("(") + toStrN(color[1].step, 4) + String(")"));
  lcd.setCursor(0, 2);
  lcd.print(String("B=") + toStrN(color[2].encValue, 5) + String("(") + toStrN(color[2].step, 4) + String(")"));
  lcd.setCursor(0, 3);
  lcd.print(String("A=") + toStrN(color[3].encValue, 5) + String("(") + toStrN(color[3].step, 4) + String(")"));

  lcd.setCursor(13, 0);
  lcd.print(String("Color="));
  lcd.setCursor(14, 1);
  lcd.print(curColor.name);
  updateTemp();
}
//####################################################################
void updateTemp() {
  lcd.setCursor(13, 3);
//  lcd.print(String("T=") + String(fTemp) + (char)223);
  lcd.print(String("T=") + String(fTemp));
}
//####################################################################
int getNextStep(int step) { 
  if (step == stepTiny)
     step = stepSmall;
  else if (step == stepSmall)
     step = stepBig;
  else if (step == stepBig)
     step = stepExtraBig;
  else if (step == stepExtraBig)
     step = stepHuge;
  else if (step == stepHuge)
     step = stepTiny;
  return step;
}
//####################################################################
int getNextColorIndex(int direction, int curColorIndex) {
  if (direction == +1) {
    if (curColorIndex < NUM_COLORS-1) {
      curColorIndex++;
    } else {
      curColorIndex = 0;
    }
  } else { // direction == -1
    if (curColorIndex == 0) {
      curColorIndex = NUM_COLORS-1;
    } else {
      curColorIndex--;
    }
  }
  
  return curColorIndex;  
}
//####################################################################
void loop() {
  if (dtemp.isConversionComplete()) {
    fTemp=dtemp.getTempCByIndex(0);
    dtemp.requestTemperatures();
    updateDisplay();
  }
  int code = encoders.readAll();
  if (code != 0) {
    curColor = color[curColorIndex];
    if (code == 1) curColor.step = getNextStep(curColor.step);
    if (code == 100) {
      if (curColor.encValue > curColor.step) {
        curColor.encValue -= curColor.step;
      } else {
        curColor.encValue = 0;
      }
    }
    if (code == 101 && curColor.encValue < maxValue) {
      curColor.encValue += curColor.step;
      Serial.println(curColor.encValue);
      if (curColor.encValue > maxValue) curColor.encValue = maxValue;
    }
    color[curColorIndex] = curColor;
    
    if (code == 200) {
      curColorIndex = getNextColorIndex(-1, curColorIndex);
      curColor = color[curColorIndex];
    }
    if (code == 201) {
      curColorIndex = getNextColorIndex(+1, curColorIndex);
      curColor = color[curColorIndex];
    }
    updateDisplay();
  }
}
