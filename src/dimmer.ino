#define VERSION "as1_dimmer_8_noTempCorr"

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

uint16_t enc1;
uint16_t enc2;
uint16_t enc3;
const uint16_t maxValue =  1023; //10-bit resolution

int stepSmall = 1;
int stepBig = 4;
int stepExtraBig = 16;

struct colorStruct {
  int pin;
  char name[6];
  uint16_t encValue;
  uint16_t value;
  int step;
  float T1;
  float T2;
  float Y1;
  float Y2;
};

#define NUM_COLORS 4

struct colorStruct color[NUM_COLORS] = {
{ 9, "Red  ", 0, 0, stepExtraBig, 16.8, 39.44, 296.045, 234.2}, // tempcorr data of 17.05.2022
{10, "Green", 0, 0, stepExtraBig, 16.75, 38.0, 614.69, 599.0}, // tempcorr data 17.05.2022
{ 5, "Blue ", 0, 0, stepExtraBig, 19.0, 39.44, 91.5, 88.3}, // tempcorr data 17.05.2022
{11, "Amber", 0, 0, stepExtraBig, 15.2, 38.81, 304.75, 206.6}, // tempcorr data 17.05.2022
};

int curColorIndex = 0;
struct colorStruct curColor = color[curColorIndex];

float fTemp;
float fTempCorrRed;
float fTempCorrGreen;
float fTempCorrAmber;

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

/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11 */
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

  setupPWM10();
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
String toStrN(int n, uint16_t i) {
  String result = "";
  for (int pos=1; pos < n; pos++) {
    if (i < pow(10,pos)) result += "0";
  }
  result += i;
  return result;
}
//####################################################################
void setBrightness() {
  for (int i = 0; i < NUM_COLORS; i++) {
    uint16_t val;
//    val = color[i].encValue * tempCorr(fTemp, color[i].T1, color[i].T2, color[i].Y1, color[i].Y2);
    val = color[i].encValue;
    if (val > maxValue) val = maxValue;
    color[i].value = val;
    analogWriteHR(color[i].pin, val);
  }
}
//####################################################################
void updateDisplay() {
  setBrightness();
  int N = 4;
//  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String("R=") + toStrN(N, color[0].value) + String("(") + toStrN(2, color[0].step) + String(")"));
  lcd.setCursor(10, 0);
  lcd.print(String("G=") + toStrN(N, color[1].value) + String("(") + toStrN(2, color[1].step) + String(")"));

  lcd.setCursor(0, 1);
  lcd.print(String("B=") + toStrN(N, color[2].value) + String("(") + toStrN(2, color[2].step) + String(")"));
  lcd.setCursor(10, 1);
  lcd.print(String("A=") + toStrN(N, color[3].value) + String("(") + toStrN(2, color[3].step) + String(")"));

  lcd.setCursor(0, 2);
  lcd.print(String("Color=") + curColor.name);
  updateTemp();
}
//####################################################################
void updateTemp() {
  lcd.setCursor(0, 3);
  lcd.print(String("T=") + String(fTemp) + (char)223);
}
//####################################################################
float tempCorr(float fTemp, float T1, float T2, float Y1, float Y2) {
  float A;
  float Y;
  if (T2 == T1)
    A = 0;
  else
    A = (Y1 - Y2)/(T2 - T1);
  Y = A * (T2 -fTemp) + Y2;
  if (fTemp < T1) Y = Y1;
  if (fTemp > T2) Y = Y2;
  if (Y != 0.0)
    return Y2/Y;
  else
    return 0.0;
}
//####################################################################
float tempCorrRed(float fTemp) {
  return tempCorr(fTemp, 10.1, 43.0, 246.0, 183.0);
}
//####################################################################
float tempCorrGreen(float fTemp) {
  return tempCorr(fTemp, 10.3, 43.2, 65.0, 65.0);
}
//####################################################################
float tempCorrAmber(float fTemp) {
  return tempCorr(fTemp, 9.4, 42.1, 252, 158);
}
//####################################################################
int getNextStep(int step) { 
  if (step == stepSmall)
     step = stepBig;
  else if (step == stepBig)
     step = stepExtraBig;
  else if (step == stepExtraBig)
     step = stepSmall;
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
    fTempCorrRed = tempCorrRed(fTemp);
    fTempCorrGreen = tempCorrGreen(fTemp);
    fTempCorrAmber = tempCorrAmber(fTemp);
    dtemp.requestTemperatures();
    updateDisplay();
  }
  int code = encoders.readAll();
  if (code != 0) {
    //Serial.println(code);
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
