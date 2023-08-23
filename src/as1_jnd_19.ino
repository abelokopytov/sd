#define VERSION "as1_jnd_19"
// no Yellow Cellophan filter
// no temp corr
// 16-bit!!!
// Thick common +12V wire in the cable


// Ambient Temp 22C, inside SB1 temp 24C (R=G=B=A=0)

#include <OneWire.h> 
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 12 
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature dtemp(&oneWire);

#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // include i/o class header
// declare the lcd object
hd44780_I2Cexp lcd; // auto locate and autoconfig interface pins
// tell the hd44780 sketch the lcd object has been declared
#define HD44780_LCDOBJECT
#define LCD_COLS 20
#define LCD_ROWS 4

#include <CommonBusEncoders.h>
CommonBusEncoders encoders(A0, A1, A2, 3);

uint16_t enc1;
uint16_t enc2;
uint16_t enc3;

#include <SimpleTimer.h>
// There must be one global SimpleTimer object.
SimpleTimer timer;
int timerId;
int timerInterval = 1500;
int phase = 0;

int screen = 0;

const uint16_t maxValue =  65535; //16-bit resolution

int rgbStepTiny = 1;
int rgbStepSmall = 4;
int rgbStepBig = 16;
//int rgbStepExtraBig = 16;
int rgbStepExtraBig = 256;

float xyStepTiny = 0.0005;
float xyStepSmall = 0.01;
float xyStep = xyStepSmall;

int phiStep = 45;
float drStep = 0.0005;

struct colorStruct {
  int pin;
  char name[6];
  uint16_t value;
  uint16_t topValue;
  int rgbStep;
};

//#define NUM_COLORS 4
#define NUM_COLORS 3

struct colorStruct color[NUM_COLORS] = {
{ 9, "Red  ", 0, 0, rgbStepExtraBig},
{10, "Green", 0, 0, rgbStepExtraBig},
{ 5, "Blue ", 0, 0, rgbStepExtraBig}
//{11, "Amber", 0, 0, rgbStepExtraBig}
};

int curColorIndex = 0;
struct colorStruct curColor = color[curColorIndex];

//matrix m[row][column] - XYZ of primaries RGB with values=65535
float m[3][3] = { // m[RGB][XYZ]. Ambient Temperature is 24C degrees
// 16-bit
//CIE 2 degree 1931 observer - file AS1_SB1_NoYF_31102022.ods
//  {611.9253795, 271.2630095,   0.291453},
//  {118.8883945, 629.305112,  142.2857565},
//  {374.525644,   91.020994, 1994.271937}
//CIE 10 degree 1964 observer - file AS1_SB1_NoYF.ods 15.02.2023
  {563.956384,  261.208255,     0.219686},
  {159.9115495, 679.5229565,  110.187244},
  {392.530064,  166.916778,  2117.5912585}
};

float topY; // max Blue
float topY1; // max ???

//inverse matrix
float minv[3][3];

struct Primary {
  int valueRed;
  int valueGreen;
  int valueBlue;
  float x;
  float y;
  float Y;
};

struct Primary primRed   = {1, 0, 0, 0.0, 0.0, 0.0};
struct Primary primGreen = {0, 1, 0, 0.0, 0.0, 0.0};
struct Primary primBlue  = {0, 0, 1, 0.0, 0.0, 0.0};

struct Vector_XYZ {
  float X;
  float Y;
  float Z;
};

struct struct_xyY {
  float x;
  float y;
  float Y;
};

struct struct_xyY xyY_Ref = {0.0, 0.0, 0.0};
struct struct_xyY xyY_Test = {0.0, 0.0, 0.0};
struct struct_xyY s = {0.0, 0.0, 0.0};

struct struct_RGB {
  uint16_t r;
  uint16_t g;
  uint16_t b;
};

struct struct_RGB rgb_Ref;
struct struct_RGB rgb_Test;

int phi = 0; //angle
float dr = 0.0; 

struct Triangle {
  float xR; //primary R
  float yR;
  float xG;
  float yG;
  float xB;
  float yB;
};

struct Triangle tr;

float fTemp;

//Configure digital pins 9, 10, 11, 5 as 10-bit PWM outputs.
//########################################
// for Leonardo
/*
void setupPWM10() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) // non-inverting PWM
        | _BV(WGM11) | _BV(WGM10);                 // mode 3: Phase correct, 10-bit
  TCCR1B = _BV(CS11); //prescaler=8 Freq = 976Hz

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1)                             // non-inverting PWM
        | _BV(WGM31) | _BV(WGM30);                 // mode 3: Phase correct, 10-bit
  TCCR3B = _BV(CS31); //prescaler=8 Freq = 976Hz
}
*/
//########################################
// Configure digital pins 9, 10, 11 and 5 as 16-bit PWM outputs.
// for Leonardo
void setupPWM16() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) // non-inverting PWM
        | _BV(WGM11);                              // mode 14: fast PWM, TOP=ICR1
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);    //prescaler=1
  ICR1 = 0xFFFF;

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1)                             // non-inverting PWM
        | _BV(WGM11);                              // mode 14: fast PWM, TOP=ICR3
  TCCR3B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);    //prescaler=1
  ICR3 = 0xFFFF;
}
//########################################
/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11, 5 */
void analogWriteHR(uint8_t pin, uint16_t val) {
  switch (pin) {
    case  9: OCR1A = val; break; //Red
    case 10: OCR1B = val; break; //Green
    case 11: OCR1C = val; break; //Amber
    case  5: OCR3A = val; break; //Blue
  }
}
//######################################
void calcInverseMatrix() {
  float det = m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) -
               m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
               m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

  float invdet = 1.0 / det;

  minv[0][0] = (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * invdet;
  minv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invdet;
  minv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invdet;
  minv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invdet;
  minv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invdet;
  minv[1][2] = (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * invdet;
  minv[2][0] = (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * invdet;
  minv[2][1] = (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * invdet;
  minv[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * invdet;
}
//####################################################################
void calc_primary_cie_xyY(Primary &prim) {
  float X = 0.0;
  float Y = 0.0;
  float Z = 0.0;
  float sumXYZ;
  X = m[0][0]*prim.valueRed + m[1][0]*prim.valueGreen + m[2][0]*prim.valueBlue;
  Y = m[0][1]*prim.valueRed + m[1][1]*prim.valueGreen + m[2][1]*prim.valueBlue;
  Z = m[0][2]*prim.valueRed + m[1][2]*prim.valueGreen + m[2][2]*prim.valueBlue;
  sumXYZ = X + Y + Z;
  prim.x = X / sumXYZ;
  prim.y = Y / sumXYZ;
  prim.Y = Y;
}
//####################################################################
float redValue_to_Y_corrected(float rel_value) {
  float rel_Y;
  rel_Y = -0.10368*rel_value*rel_value + 1.10248*rel_value + 0.0005;
  return rel_Y;
}
//####################################################################
float Y_to_redValue_corrected(float rel_Y) {
  float a = -0.10368;
  float b = 1.10248;
  float c = 0.0005;
  float det;
  float rel_value;
  det = b*b - 4*a*(c - rel_Y);
  rel_value = (-b + sqrt(det))/a*0.5;
  return rel_value;
}
//####################################################################
void rgb_to_cie_xyY(struct_RGB RGB, struct_xyY &xyY) {
  uint16_t value;
  float X = 0.0;
  float Y = 0.0;
  float Z = 0.0;
  float sumXYZ;
  float maxRedY=m[0][1];
  float rel_R_corr = redValue_to_Y_corrected(1.0/maxValue*RGB.r);
//  float rel_R_corr = 1.0*RGB.R/maxValue;
  float rel_G = 1.0/maxValue*RGB.g;
  float rel_B = 1.0/maxValue*RGB.b;
  X = m[0][0]*rel_R_corr + m[1][0]*rel_G + m[2][0]*rel_B;
  Y = m[0][1]*rel_R_corr + m[1][1]*rel_G + m[2][1]*rel_B;
  Z = m[0][2]*rel_R_corr + m[1][2]*rel_G + m[2][2]*rel_B;
  sumXYZ = X + Y + Z;
  xyY.x = X / sumXYZ;
  xyY.y = Y / sumXYZ;
  xyY.Y = Y;
}
//####################################################################
/*
void cie_xyY_to_XYZ(Vector_xyY &xyY, Vector_XYZ &XYZ) {
  if (xyY.Y == 0) {
    XYZ.X = 0.0;
    XYZ.Y = 0.0;
    XYZ.Z = 0.0;
    return;
  }
  XYZ.X = xyY.x*xyY.Y/xyY.y;
  XYZ.Y = xyY.Y;
  XYZ.Z = (1 - xyY.x - xyY.y)*xyY.Y/xyY.y;
}
//####################################################################
void cie_XYZ_to_RGB(Vector_XYZ XYZ, Vector_RGB &RGB) {
  RGB.R = round((minv[0][0]*XYZ.X + minv[1][0]*XYZ.Y + minv[2][0]*XYZ.Z) * maxValue);
  RGB.G = round((minv[0][1]*XYZ.X + minv[1][1]*XYZ.Y + minv[2][1]*XYZ.Z) * maxValue);
  RGB.B = round((minv[0][2]*XYZ.X + minv[1][2]*XYZ.Y + minv[2][2]*XYZ.Z) * maxValue);
}
*/
//####################################################################
void cie_xyY_to_rgb(struct_xyY xyY, struct_RGB &RGB) {
  float RedY;
  Vector_XYZ XYZ;
  if (xyY.Y == 0.0) {
    RGB.r = 0.0;
    RGB.g = 0.0;
    RGB.b = 0.0;
    return;
  }
  XYZ.X = xyY.x*xyY.Y/xyY.y;
  XYZ.Y = xyY.Y;
  XYZ.Z = (1 - xyY.x - xyY.y)*xyY.Y/xyY.y;
  uint16_t R = round((minv[0][0]*XYZ.X + minv[1][0]*XYZ.Y + minv[2][0]*XYZ.Z) * maxValue);
  RGB.g = round((minv[0][1]*XYZ.X + minv[1][1]*XYZ.Y + minv[2][1]*XYZ.Z) * maxValue);
  RGB.b = round((minv[0][2]*XYZ.X + minv[1][2]*XYZ.Y + minv[2][2]*XYZ.Z) * maxValue);
  RedY = XYZ.Y - (m[1][1]*RGB.g + m[2][1]*RGB.b)/maxValue;
  int32_t Rcorr = round(Y_to_redValue_corrected(RedY/m[0][1])*maxValue);
  if (Rcorr < 0) RGB.r = 0;
  else RGB.r = Rcorr;
}
//####################################################################
void setup_triangle() {
  tr.xR = primRed.x;
  tr.yR = primRed.y;
  tr.xG = primGreen.x;
  tr.yG = primGreen.y;
  tr.xB = primBlue.x;
  tr.yB = primBlue.y;
}
//####################################################################
//http://totologic.blogspot.com/2014/01/accurate-point-in-triangle-test.html
bool pointInTriangle(float x, float y) {
  float x1; float y1; float x2; float y2; float x3; float y3;
  x1 = tr.xR, y1 = tr.yR;
  x2 = tr.xG; y2 = tr.yG;
  x3 = tr.xB; y3 = tr.yB;
  float denominator = ((y2 - y3)*(x1 - x3) + (x3 - x2)*(y1 - y3));
  float a = ((y2 - y3)*(x - x3) + (x3 - x2)*(y - y3)) / denominator;
  float b = ((y3 - y1)*(x - x3) + (x1 - x3)*(y - y3)) / denominator;
  float c = 1 - a - b;
  return 0 <= a && a <= 1 && 0 <= b && b <= 1 && 0 <= c && c <= 1;
}
//####################################################################
void drphi2xy(float dr, int phi, struct_xyY &s1) {
  float rad = phi / 57.296;
  s1.x += float(dr*cos(rad));
  s1.y += float(dr*sin(rad));
}
//####################################################################
void setup() {
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


//  setupPWM10();
  setupPWM16();
  updateDisplay();
  
  topY = m[2][1]; // max Blue
  topY1=2.0*topY;
//  color[0].topValue = topY/m[0][1]*maxValue; //Red
//  color[0].topValue = Y_to_redValue_corrected(topY/m[0][1])*maxValue; //Red
  color[0].topValue = Y_to_redValue_corrected(topY1/m[0][1])*maxValue; //Red
//  color[1].topValue = topY/m[1][1]*maxValue; //Green
  color[1].topValue = topY1/m[1][1]*maxValue; //Green
  color[2].topValue = maxValue; //Blue

  calcInverseMatrix();
  calc_primary_cie_xyY(primRed);
  calc_primary_cie_xyY(primGreen);
  calc_primary_cie_xyY(primBlue);
  setup_triangle();
//  Serial.println("primRed.cie_xy="+String(primRed.x)+";"+String(primRed.y));
//  Serial.println("primGreen.cie_xy="+String(primGreen.x)+";"+String(primGreen.y));
//  Serial.println("primBlue.cie_xy="+String(primBlue.x)+";"+String(primBlue.y));
/*
  Serial.println('m=');
  for (int row=0; row<3; row++) {
    Serial.println('[' + String(m[row][0]) + ' ' + String(m[row][1])+ ' ' + String(m[row][2])+ ']');
  }
  Serial.println('minv=');
  for (int row=0; row<3; row++) {
    Serial.println('[' + String(minv[row][0],5) + ' ' + String(minv[row][1],5)+ ' ' + String(minv[row][2],5)+ ']');
  }
*/
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
  if (phase == 0) {          //Reference Stimulus
    analogWriteHR(color[0].pin, rgb_Ref.r);
    analogWriteHR(color[1].pin, rgb_Ref.g);
    analogWriteHR(color[2].pin, rgb_Ref.b);
  }
  if (phase == 1) {          //Test Stimulus
    analogWriteHR(color[0].pin, rgb_Test.r);
    analogWriteHR(color[1].pin, rgb_Test.g);
    analogWriteHR(color[2].pin, rgb_Test.b);
  }
  if (timer.isEnabled(timerId)) {
    phase = 1 - phase;
  }
}
//####################################################################
void updateDisplay() {
  if (! timer.isEnabled(timerId)) {
    setBrightness();
  }
  //Serial.println("R="+String(RGB.R)+"; G="+String(RGB.G)+"; B="+String(RGB.B));
  if (screen == 0) {
    lcd.setCursor(15, 0);
    lcd.print("Scr0");
    rgb_Ref.r = color[0].value;
    rgb_Ref.g = color[1].value;
    rgb_Ref.b = color[2].value;
    rgb_to_cie_xyY(rgb_Ref, xyY_Ref);
    
    lcd.setCursor(0, 0);
    lcd.print("R" + toStrN(color[0].value, 5) + "(" + toStrN(color[0].rgbStep, 2) + ")");
    lcd.setCursor(0, 1);
    lcd.print("G" + toStrN(color[1].value, 5) + "(" + toStrN(color[1].rgbStep, 2) + ")");
    lcd.setCursor(0, 2);
    lcd.print("B" + toStrN(color[2].value, 5) + "(" + toStrN(color[2].rgbStep, 2) + ")");
  //  lcd.setCursor(10, 1);
  //  lcd.print("A=" + toStrN(color[3].value, N) + "(" + toStrN(color[3].rgbStep, 2) + ")");

    lcd.setCursor(0, 3);
    lcd.print("C=" + String(curColor.name));

    lcd.setCursor(12, 2);
    lcd.print("Y=" + String(xyY_Ref.Y));
    if (xyY_Ref.Y < 100.0) lcd.print(" ");
    if (xyY_Ref.Y < 10.0) lcd.print(" ");
    /*
    lcd.setCursor(0, 2);
    lcd.print(String("x=") + String(xyY.x, 3) + " ");
    lcd.setCursor(0, 3);
    lcd.print(String("y=") + String(xyY.y, 3) + " ");
    */
  }  
  if (screen == 1) {
    lcd.setCursor(15, 0);
    lcd.print("Scr1");
    cie_xyY_to_rgb(xyY_Ref, rgb_Ref);
    color[0].value = rgb_Ref.r;
    color[1].value = rgb_Ref.g;
    color[2].value = rgb_Ref.b;
    lcd.setCursor(12, 2);
    lcd.print("Y=" + String(xyY_Ref.Y));
    lcd.setCursor(0, 2);
    lcd.print("x=" + String(xyY_Ref.x, 3) + " ");
    lcd.setCursor(0, 3);
    lcd.print("y=" + String(xyY_Ref.y, 3) + " ");
  }
  if (screen == 2) {
    lcd.setCursor(15, 0);
    lcd.print("Scr2");
    cie_xyY_to_rgb(xyY_Test, rgb_Test);
    lcd.setCursor(12, 1);
    lcd.print("dY=" + String(xyY_Test.Y - xyY_Ref.Y));
    lcd.setCursor(0, 0);
    lcd.print("dx=" + String(xyY_Test.x - xyY_Ref.x, 4)+ " ");
    lcd.setCursor(0, 1);
    lcd.print("dy=" + String(xyY_Test.y - xyY_Ref.y, 4)+ " ");
    lcd.setCursor(0, 2);
    lcd.print("phi=" + String(phi) + "  ");
    lcd.setCursor(0, 3);
    lcd.print("dr=" + String(dr, 4));
    
    lcd.setCursor(12, 2);
    lcd.print("Y=" + String(xyY_Ref.Y));
  }
  updateTemp();
}
//####################################################################
void updateTemp() {
  lcd.setCursor(12, 3);
  lcd.print("T=" + String(fTemp) + (char)223);
}
//####################################################################
int getNextRgbStep(int step) { 
  if (step == rgbStepTiny)
     step = rgbStepSmall;
  else if (step == rgbStepSmall)
     step = rgbStepBig;
  else if (step == rgbStepBig)
     step = rgbStepExtraBig;
  else if (step == rgbStepExtraBig)
     step = rgbStepTiny;
  return step;
}
//####################################################################
int getNextColorIndex(int direction, int curColorIndex) {
  if (direction == +1) {
    if (curColorIndex < NUM_COLORS-1) curColorIndex++;
    else curColorIndex = 0;
  } else { // direction == -1
    if (curColorIndex == 0) curColorIndex = NUM_COLORS-1;
    else curColorIndex--;
  }
  return curColorIndex;  
}
//####################################################################
void loop() {
  if (screen == 2) {
    timer.run();
  }
  if (dtemp.isConversionComplete()) {
    fTemp=dtemp.getTempCByIndex(0);
    dtemp.requestTemperatures();
    updateDisplay();
  }
  int code = encoders.readAll();
  if (code != 0) {
    //######################### screen 0: RGB
    if (screen == 0) {
      curColor = color[curColorIndex];
      if (code == 1) curColor.rgbStep = getNextRgbStep(curColor.rgbStep);
      if (code == 100) { //--
        if (curColor.value > curColor.rgbStep)
          curColor.value -= curColor.rgbStep;
        else
          curColor.value = 0;
      }
      if (code == 101) { //++
        if (curColor.value <= curColor.topValue - curColor.rgbStep)
          curColor.value += curColor.rgbStep;
        else curColor.value = curColor.topValue;
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
    }
    //######################### screen 1: xyY Reference
    if (screen == 1) {
      if (code == 1 || code == 2) {
        if (xyStep == xyStepTiny) xyStep = xyStepSmall;
        else if (xyStep == xyStepSmall) xyStep = xyStepTiny;
      }
      if (code == 100) { // x-
        s = xyY_Ref;
        s.x -= xyStep;
        if (pointInTriangle(s.x, s.y)) xyY_Ref.x -= xyStep;
      }
      if (code == 101) { // x+
        s = xyY_Ref;
        s.x += xyStep;
        if (pointInTriangle(s.x, s.y)) xyY_Ref.x += xyStep;
      }
      if (code == 200) { // y-
        s = xyY_Ref;
        s.y -= xyStep;
        if (pointInTriangle(s.x, s.y)) xyY_Ref.y -= xyStep;
      }
      if (code == 201) { // y+
        s = xyY_Ref;
        s.y += xyStep;
        if (pointInTriangle(s.x, s.y)) xyY_Ref.y += xyStep;
      }
      if (code == 300) { // Y-
        if (xyY_Ref.Y > 5.0) {
          xyY_Ref.Y -= 0.1;
        } else {
          xyY_Ref.Y = 0.0;
        }
      }
      if (code == 301) { // Y+
        if (xyY_Ref.Y < topY1) {
          xyY_Ref.Y += 0.1;
        } else {
          xyY_Ref.Y = topY1;
        }
      }
    }
    //######################### screen 2: xyY Test 
    if (screen == 2) {
      if (code == 100) { // phi-
        phi -= phiStep;
        if (phi == - phiStep) phi = 360 - phiStep;
        dr = 0;
        xyY_Test.x = xyY_Ref.x;
        xyY_Test.y = xyY_Ref.y;
      }
      if (code == 101) { // phi+
        phi += phiStep;
        if (phi == 360) phi = 0;
        dr = 0;
        xyY_Test.x = xyY_Ref.x;
        xyY_Test.y = xyY_Ref.y;
      }
      if (code == 200) { // dr-
        if (dr >= drStep) {
          s.x = xyY_Ref.x;
          s.y = xyY_Ref.y;
          drphi2xy(dr - drStep, phi, s);
          if (pointInTriangle(s.x, s.y)) {
            dr -= drStep;
            xyY_Test.x = s.x;
            xyY_Test.y = s.y;
          }
        } else {
          dr = 0.0;
          xyY_Test.x = xyY_Ref.x;
          xyY_Test.y = xyY_Ref.y;
        }
      }
      if (code == 201) { // dr+
        s.x = xyY_Ref.x;
        s.y = xyY_Ref.y;
        drphi2xy(dr + drStep, phi, s);
        if (pointInTriangle(s.x, s.y)) {
          dr += drStep;
          xyY_Test.x = s.x;
          xyY_Test.y = s.y;
        }
      }
      if (code == 300) { // Y_Test-
        if (xyY_Test.Y >= 0.1) {
          xyY_Test.Y -= 0.1;
        } else {
          xyY_Test.Y = 0.0;
        }
      }
      if (code == 301) { // Y_Test+
        if (xyY_Test.Y < topY1 - 0.1) {
          xyY_Test.Y += 0.1;
        } else {
          xyY_Test.Y = topY1;
        }
      }
    }
    // ###########################
    if (screen == 0 || screen == 1) {
      rgb_Ref.r = color[0].value;
      rgb_Ref.g = color[1].value;
      rgb_Ref.b = color[2].value;
    }
    //#########################
    if (code == 3) {
      if (screen == 0) {
        screen = 1;
      }
      else if (screen == 1) {
        screen = 2;
        rgb_Test = rgb_Ref;
        xyY_Test = xyY_Ref;
        timerId = timer.setInterval(timerInterval, setBrightness);
      }
      else if (screen == 2) {
        screen = 0;
        timer.deleteTimer(timerId);
        phase = 0;
        rgb_Test = rgb_Ref;
      }
      lcd.clear();
    }
    updateDisplay();
  } //(code != 0)
}
