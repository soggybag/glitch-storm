// Glitch Storm
// CC By Sa Spherical Sound Society 2020
// Heavy inspiration in Bytebeat (Viznut)
// Some equations are empty. You can collaborate sending your new finding cool sounding ones to the repository
// https://github.com/spherical-sound-society/glitch-storm

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "equations.h"

#define isDebugging   true

#define ledPin        13
#define speakerPin    11
#define upButtonPin   2
#define downButtonPin 4
#define progBit0Pin   7
#define progBit1Pin   8
#define progBit2Pin   9
#define progBit3Pin   10

int debounceRange = 20;

long t = 0;
volatile int a, b, c;
volatile int value;

byte programNumber = 1;
byte upButtonState = 0;
byte downButtonState = 0;
byte lastButtonState = 0;
byte totalPrograms = 16;
byte clocksOut = 0;

int cyclebyte = 0;
volatile int aTop = 99;
volatile int aBottom = 0;
volatile int bTop = 99;
volatile int bBottom = 0;
volatile int cTop = 99;
volatile int cBottom = 0;

int d = 0;

bool isClockOutMode = false;
bool isSerialValues = true;

static unsigned long time_now = 0;

long button1Timer = 0;
long longPress1Time = 400;
long button2Timer = 0;
long longPress2Time = 400;

boolean isButton1Active = false;
boolean isLongPress1Active = false;
boolean isButton2Active = false;
boolean isLongPress2Active = false;

int  shift_A_Pot = 1;
int  old_A_Pot = 1;
int SAMPLE_RATE = 16384;
int old_SAMPLE_RATE = SAMPLE_RATE;
byte shift_C_Pot = 0;
byte old_C_Pot = 0;


void initSound() {
  pinMode(speakerPin, OUTPUT);
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);

  // Do non-inverting PWM on pin OC2A (p.155)
  // On the Arduino this is pin 11.
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  // Set initial pulse width to the first sample.
  OCR2A = 0;

  // Set up Timer 1 to send a sample every interrupt.
  cli();

  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe.
  OCR1A = F_CPU / SAMPLE_RATE;    // 16e6 / 8000 = 2000
  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A);

  sei();
}

void buttonsManager() {
  bool pressBothButtons = false;
  //start button 1
  if (digitalRead(upButtonPin) == LOW) {
    if (isButton1Active == false) {
      isButton1Active = true;
      button1Timer = millis();
      // Serial.println("RIGHT button short press");
    }
    if ((millis() - button1Timer > longPress1Time) && (isLongPress1Active == false)) {
      isLongPress1Active = true;
      // Serial.println("RIGHT long press ON");
    }
  } else {
    if (isButton1Active == true) {
      if (isLongPress1Active == true) {
        isLongPress1Active = false;

        // Serial.println("RIGHT long press RELEASE");
      } else {

        if (programNumber != totalPrograms) {
          programNumber++;
        }
        else if (programNumber == totalPrograms) {
          programNumber = 1;
        }
        // Serial.println("RIGHT button short release");
        // Serial.print("PROGRAM: ");
        // Serial.println(programNumber);
        ledCounter();
      }
      isButton1Active = false;
    }
  }
  // end RIGHT button
  // start LEFT button
  if (digitalRead(downButtonPin) == LOW) {
    if (isButton2Active == false) {
      isButton2Active = true;
      button2Timer = millis();
      // Serial.println("LEFT button short press");
    }
    if ((millis() - button2Timer > longPress2Time) && (isLongPress2Active == false)) {
      isLongPress2Active = true;

      // Serial.println("LEFT BUTTON long press ON");
    }
  } else {
    if (isButton2Active == true) {
      if (isLongPress2Active == true) {
        isLongPress2Active = false;
        // Serial.println("LEFT BUTTON long press release");
        pressBothButtons = true;
        // isClockOutMode = !isClockOutMode;
        // we only change program in short pressed, not long ones
        programNumber++;

      } else {
        if (downButtonState == LOW) {
          if (programNumber > 1) {
            programNumber--;
          } else if (programNumber == 1) {
            programNumber = totalPrograms;
          }
          // Serial.println("LEFT BUTTON short release");
        }
        ledCounter();
        isButton2Active = false;
      }

    }
    
    //end button 2
    if (!isLongPress2Active && isLongPress1Active && pressBothButtons) {
      // Serial.println("HACKKK");
      isClockOutMode = !isClockOutMode;
    }
  }
}

// SETUP! /////////////////////////////////////////////////////
void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(progBit0Pin, OUTPUT);
  pinMode(progBit1Pin, OUTPUT);
  pinMode(progBit2Pin, OUTPUT);
  pinMode(progBit3Pin, OUTPUT);

  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);

  initSound();
  ledCounter();

  if (isDebugging) {
    Serial.begin(9600);
  }
}

// LOOP! //////////////////////////////////////////////////////
void loop() {
  buttonsManager();
  potsManager();

  //slow loop show serial once every second
  if (isDebugging) {
    if (millis() > time_now + 1000) {
      time_now = millis();
      printValues();
    }
  }
}

uint8_t scaleParam(int analogValue, uint8_t minVal, uint8_t maxVal) {
  return map(analogValue, 0, 1023, minVal, maxVal);
}

void potsManager() {
  int analogA = analogRead(A0); // A0
  int analogB = analogRead(A1); // A1
  int analogC = analogRead(A2); // A2

  // a = map(analogRead(0), 0, 1023, aBottom, aTop);

  /*uint8_t*/ a = scaleParam(analogA, equations[programNumber].aMin, equations[programNumber].aMax);
  /*uint8_t*/ b = scaleParam(analogB, equations[programNumber].bMin, equations[programNumber].bMax);
  /*uint8_t*/ c = scaleParam(analogC, equations[programNumber].cMin, equations[programNumber].cMax);

  // Serial.println(analogRead(3));
  c = (cBottom + cTop) >> 1;
  SAMPLE_RATE = map(analogRead(3), 0, 1023, 256, 32768);

  // this only if sample rate is different.
  OCR1A = F_CPU / SAMPLE_RATE;
}

void rightLongPressActions() {

  //REVERSE TIME *********************
  int actual_A_Pot = map(analogRead(0), 0, 1023, -7, 7);

  if (old_A_Pot != actual_A_Pot) {

    shift_A_Pot = actual_A_Pot;
  }
  old_A_Pot = actual_A_Pot;


  if (shift_A_Pot == 0) {
    //prevents the engine to stop
    shift_A_Pot = 1;
  }
}
void leftLongPressActions() {
  // SAMPLE RATE *************************************

  old_SAMPLE_RATE = SAMPLE_RATE;
  //int actual_SAMPLE_RATE = analogRead(1);
  SAMPLE_RATE = softDebounce(analogRead(0), SAMPLE_RATE);

  // actual_SAMPLE_RATE=map(analogRead(1), 0, 1023, 256, 16384);
  if (SAMPLE_RATE != old_SAMPLE_RATE) {
    //el mapeo se hace aqui
    //map(analogRead(1), 0, 1023, 256, 16384);
    int mappedSAMPLE_RATE = map(SAMPLE_RATE, 0, 1023, 256, 16384);
    OCR1A = F_CPU / mappedSAMPLE_RATE;
  }

}

void ledCounter() {
  int val;
  if (isClockOutMode) {
    //show clocks
    clocksOut++;
    if (clocksOut == 16) {
      clocksOut = 0;
    }
    val = clocksOut;
  } else {
    //show program number in binary
    val = programNumber;
  }
  digitalWrite(progBit0Pin, bitRead(val, 0));
  digitalWrite(progBit1Pin, bitRead(val, 1));
  digitalWrite(progBit2Pin, bitRead(val, 2));
  digitalWrite(progBit3Pin, bitRead(val, 3));
}

void printValues() {
  Serial.print("program: ");
  Serial.print(programNumber);
  Serial.print("  a: ");
  Serial.print(a);
  Serial.print("  b: ");
  Serial.print(b);
  Serial.print("  c: ");
  Serial.println(c);

  Serial.print("A0: "); Serial.print(analogRead(A0));
  Serial.print("  A1: "); Serial.print(analogRead(A1));
  Serial.print("  A2: "); Serial.println(analogRead(A2));
}

int  softDebounce(int  readCV, int  oldRead) {
  if (abs(readCV - oldRead) > debounceRange) {
    return readCV;
  }
  return oldRead;
}

const uint8_t NUM_EQUATIONS = sizeof(equations) / sizeof(equations[0]);

ISR(TIMER1_COMPA_vect) {
  if (programNumber >= NUM_EQUATIONS) programNumber = 0; // Reset to first equation if out of bounds
  value = equations[programNumber].func(t, a, b, c);
  OCR2A = value;
  t++;
}
