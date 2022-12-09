#define VERSION "as1_jnd_11_polar"
//based on dimmer_12_xyY
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
  {611.9253795, 271.2630095,   0.291453},
  {118.8883945, 629.305112,  142.2857565},
  {374.525644,   91.020994, 1994.271937}
};

float topY; // max Blue

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

struct Vector_XYZ XYZ = {0.0, 0.0, 0.0};

struct Vector_xyY {
  float x;
  float y;
  float Y;
};

struct Vector_xyY xyY = {0.0, 0.0, 0.0};
struct Vector_xyY xyY_Test = {0.0, 0.0, 0.0};
struct Vector_xyY s = {0.0, 0.0, 0.0};

struct Vector_RGB {
  uint16_t R;
  uint16_t G;
  uint16_t B;
};

struct Vector_RGB RGB;
struct Vector_RGB RGB_Test;

int phi = 0; //angle
int phi1 = 0;
float dr = 0.0; 
float root1_2 = 0.707107;

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
//########################################
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
//########################################
/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11 */
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
void calc_cie_xy(Vector_RGB RGB, Vector_xyY &xyY) {
  float X = 0.0;
  float Y = 0.0;
  float Z = 0.0;
  float sumXYZ;
  for (int i = 0; i < NUM_COLORS; i++) {
    X += m[i][0]*color[i].value/maxValue;
    Y += m[i][1]*color[i].value/maxValue;
    Z += m[i][2]*color[i].value/maxValue;
  };
  sumXYZ = X + Y + Z;
  xyY.x = X / sumXYZ;
  xyY.y = Y / sumXYZ;
  xyY.Y = Y;
}
//####################################################################
void calc_cie_XYZ(Vector_xyY &xyY, Vector_XYZ &XYZ) {
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
void calc_RGB(Vector_XYZ XYZ, Vector_RGB &RGB) {
  RGB.R = round((minv[0][0]*XYZ.X + minv[1][0]*XYZ.Y + minv[2][0]*XYZ.Z) * maxValue);
  RGB.G = round((minv[0][1]*XYZ.X + minv[1][1]*XYZ.Y + minv[2][1]*XYZ.Z) * maxValue);
  RGB.B = round((minv[0][2]*XYZ.X + minv[1][2]*XYZ.Y + minv[2][2]*XYZ.Z) * maxValue);
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
void drphi2xy(float dr, int phi, Vector_xyY &s1) {
  if (phi ==   0) {s1.x += +dr;          s1.y += 0;}
  if (phi ==  45) {s1.x += +dr*root1_2;  s1.y += +dr*root1_2;}
  if (phi ==  90) {s1.x += 0;            s1.y += +dr;}
  if (phi == 135) {s1.x += -dr*root1_2;  s1.y += +dr*root1_2;}
  if (phi == 180) {s1.x += -dr;          s1.y += 0;}
  if (phi == 225) {s1.x += -dr*root1_2;  s1.y += -dr*root1_2;}
  if (phi == 270) {s1.x += 0;            s1.y += -dr;}
  if (phi == 315) {s1.x += +dr*root1_2;  s1.y += -dr*root1_2;}
}
//####################################################################
void setup() {
  // put your setup code here, to run once:
  //Serial.begin(115200);

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
  color[0].topValue = topY/m[0][1]*maxValue; //Red
  color[1].topValue = topY/m[1][1]*maxValue; //Green
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
  if (phase == 0) {          //Reference Stimul
    analogWriteHR(color[0].pin, RGB.R);
    analogWriteHR(color[1].pin, RGB.G);
    analogWriteHR(color[2].pin, RGB.B);
  }
  if (phase == 1) {          //Test Stimul
    analogWriteHR(color[0].pin, RGB_Test.R);
    analogWriteHR(color[1].pin, RGB_Test.G);
    analogWriteHR(color[2].pin, RGB_Test.B);
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
  int N = 5;
  if (screen == 0) {
    RGB.R = color[0].value;
    RGB.G = color[1].value;
    RGB.B = color[2].value;
    calc_cie_xy(RGB, xyY);
    lcd.setCursor(0, 0);
    lcd.print(String("R") + toStrN(N, color[0].value) + String("(") + toStrN(2, color[0].rgbStep) + String(")"));
    lcd.setCursor(10, 0);
    lcd.print(String("G") + toStrN(N, color[1].value) + String("(") + toStrN(2, color[1].rgbStep) + String(")"));
    lcd.setCursor(0, 1);
    lcd.print(String("B") + toStrN(N, color[2].value) + String("(") + toStrN(2, color[2].rgbStep) + String(")"));
  //  lcd.setCursor(10, 1);
  //  lcd.print(String("A=") + toStrN(N, color[3].value) + String("(") + toStrN(2, color[3].rgbStep) + String(")"));

    lcd.setCursor(12, 1);
    lcd.print(String("C=") + curColor.name);

    lcd.setCursor(12, 2);
    lcd.print(String("Y=") + String(xyY.Y));
    if (xyY.Y < 100.0) lcd.print(" ");
    if (xyY.Y < 10.0) lcd.print(" ");

    lcd.setCursor(0, 2);
    lcd.print(String("x=") + String(xyY.x, 3) + " ");
    lcd.setCursor(0, 3);
    lcd.print(String("y=") + String(xyY.y, 3) + " ");
  }  
  if (screen == 1) {
    calc_cie_XYZ(xyY, XYZ);
    calc_RGB(XYZ, RGB);
    color[0].value = RGB.R;
    color[1].value = RGB.G;
    color[2].value = RGB.B;
    //Serial.println("Screen=1 R="+String(RGB.R));
    lcd.setCursor(12, 2);
    lcd.print(String("Y=") + String(xyY.Y));
    lcd.setCursor(0, 2);
    lcd.print(String("x=") + String(xyY.x, 3) + " ");
    lcd.setCursor(0, 3);
    lcd.print(String("y=") + String(xyY.y, 3) + " ");
  }
  if (screen == 2) {
    calc_cie_XYZ(xyY_Test, XYZ);
    calc_RGB(XYZ, RGB_Test);
    lcd.setCursor(12, 0);
    lcd.print(String("dY=") + String(xyY_Test.Y - xyY.Y));
    lcd.setCursor(0, 0);
    lcd.print(String("dx=") + String(xyY_Test.x - xyY.x, 4)+ " ");
    lcd.setCursor(0, 1);
    lcd.print(String("dy=") + String(xyY_Test.y - xyY.y, 4)+ " ");
    //lcd.setCursor(0, 2);
    //lcd.print(String("x=") + String(xyY.x, 3));
    //lcd.setCursor(0, 3);
    //lcd.print(String("y=") + String(xyY.y, 3));
    lcd.setCursor(0, 2);
    lcd.print(String("phi=") + String(phi) + "  ");
    lcd.setCursor(0, 3);
    lcd.print(String("dr=") + String(dr, 4));
    
    lcd.setCursor(12, 2);
    lcd.print(String("Y=") + String(xyY.Y));

//    Serial.println("R="+String(RGB.R)+"; R_Test="+String(RGB_Test.R));
//    Serial.println("G="+String(RGB.G)+"; G_Test="+String(RGB_Test.G));
//    Serial.println("B="+String(RGB.B)+"; B_Test="+String(RGB_Test.B));

  }
  updateTemp();
}
//####################################################################
void updateTemp() {
  lcd.setCursor(12, 3);
  lcd.print(String("T=") + String(fTemp) + (char)223);
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
    if (screen == 0) {  //######################### screen 0: RGB
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
    } // screen == 0
    if (screen == 1) { //######################### screen 1: xyY Reference
      if (code == 1) {
        if (xyStep == xyStepTiny) xyStep = xyStepSmall;
        else if (xyStep == xyStepSmall) xyStep = xyStepTiny;
      }
      if (code == 100) { // x-
        s = xyY;
        s.x -= xyStep;
        //Serial.println("pointInTriangle="+String(pointInTriangle(s.x, s.y)));
        if (pointInTriangle(s.x, s.y)) {
          xyY.x -= xyStep;
        }
      }
      if (code == 101) { // x+
        s = xyY;
        s.x += xyStep;
        if (pointInTriangle(s.x, s.y)) {
          xyY.x += xyStep;
        }
      }
      if (code == 200) { // y-
        s = xyY;
        s.y -= xyStepSmall;
        if (pointInTriangle(s.x, s.y)) {
          xyY.y -= xyStep;
        }
      }
      if (code == 201) { // y+
        s = xyY;
        s.y += xyStepSmall;
        if (pointInTriangle(s.x, s.y)) {
          xyY.y += xyStep;
        }
      }
      if (code == 300) { // Y-
        if (xyY.Y > 5.0) {
          xyY.Y -= 0.1;
        } else {
          xyY.Y = 0.0;
        }
      }
      if (code == 301) { // Y+
        if (xyY.Y < topY) {
          xyY.Y += 0.1;
        } else {
          xyY.Y = topY;
        }
      }
      
    } // screen == 1
    if (screen == 2) { //######################### screen 2: xyY Test 
      if (code == 100) { // phi-
        s.x = xyY.x;
        s.y = xyY.y;
        phi1 = phi - phiStep;
        if (phi1 == - phiStep) {
          phi1 = 360 - phiStep;
        }
        drphi2xy(dr, phi1, s);
        if (pointInTriangle(s.x, s.y)) {
          xyY_Test.x = s.x;
          xyY_Test.y = s.y;
          phi = phi1;
        }
      }
      if (code == 101) { // phi+
        s.x = xyY.x;
        s.y = xyY.y;
        phi1 = phi + phiStep;
        if (phi1 == 360) {
          phi1 = 0;
        }
        drphi2xy(dr, phi1, s);
        if (pointInTriangle(s.x, s.y)) {
          xyY_Test.x = s.x;
          xyY_Test.y = s.y;
          phi = phi1;
        }
      }
      if (code == 200) { // dr-
        if (dr >= drStep) {
          s.x = xyY.x;
          s.y = xyY.y;
          drphi2xy(dr - drStep, phi, s);
          if (pointInTriangle(s.x, s.y)) {
            dr -= drStep;
            xyY_Test.x = s.x;
            xyY_Test.y = s.y;
          }
        } else {
          dr = 0.0;
          xyY_Test.x = xyY.x;
          xyY_Test.y = xyY.y;
        }
      }
      if (code == 201) { // dr+
        s.x = xyY.x;
        s.y = xyY.y;
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
        if (xyY_Test.Y < topY - 0.1) {
          xyY_Test.Y += 0.1;
        } else {
          xyY_Test.Y = topY;
        }
      }
    } // screen == 2
    if (screen == 0 || screen == 1) {
      RGB.R = color[0].value;
      RGB.G = color[1].value;
      RGB.B = color[2].value;
    }
    //#########################
    if (code == 3) {
      if (screen == 0) {
        screen = 1;
      }
      else if (screen == 1) {
        screen = 2;
        RGB_Test.R = RGB.R;
        RGB_Test.G = RGB.G;
        RGB_Test.B = RGB.B;
        xyY_Test.x = xyY.x;
        xyY_Test.y = xyY.y;
        xyY_Test.Y = xyY.Y;
        timerId = timer.setInterval(timerInterval, setBrightness);
      }
      else if (screen == 2) {
        timer.deleteTimer(timerId);
        phase = 0;
        RGB_Test.R = RGB.R;
        RGB_Test.G = RGB.G;
        RGB_Test.B = RGB.B;
        screen = 0;
      }
      lcd.clear();
    }
    updateDisplay();
  } //(code != 0)
}
