// Glitch Storm
// CC By Sa Spherical Sound Society 2020
// Heavy inspiration in Bytebeat (Viznut)
// Some equations are empty. You can collaborate sending your new finding cool sounding ones to the repository
// https://github.com/spherical-sound-society/glitch-storm

// This is my take on Glitch Storm. I revised the original files and made a few small changes to suit 
// myself in software design. 

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "equations.h"

#define isDebugging   false

#define ledPin        13
#define speakerPin    11
#define upButtonPin   2
#define downButtonPin 4
#define progBit0Pin   7
#define progBit1Pin   8
#define progBit2Pin   9
#define progBit3Pin   10

long t = 0;
volatile uint8_t a = 0, b = 0, c = 0;
volatile int value;

byte programNumber = 0;
byte upButtonState = 0;
byte downButtonState = 0;
byte clocksOut = 0;

bool isClockOutMode = false;

static unsigned long time_now = 0;

int SAMPLE_RATE = 16384;


///////////////////////////////////////////////////////////////
// SETUP AUDIO OUTPUT /////////////////////////////////////////
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

///////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////
// Handle Pots ////////////////////////////////////////////////
uint8_t scaleParam(int analogValue, uint8_t minVal, uint8_t maxVal) {
  return map(analogValue, 0, 1023, minVal, maxVal);
}

void potsManager() {
  // Get Pots for vars a, b, and c
  int analogA = analogRead(A0); // A0
  int analogB = analogRead(A1); // A1
  int analogC = analogRead(A2); // A2

  // Scale vars a, b, and c, by the ranges for each equation
  a = scaleParam(analogA, equations[programNumber].aMin, equations[programNumber].aMax);
  b = scaleParam(analogB, equations[programNumber].bMin, equations[programNumber].bMax);
  c = scaleParam(analogC, equations[programNumber].cMin, equations[programNumber].cMax);

  // Get Sample Rate Pot value and update sample rate
  SAMPLE_RATE = map(analogRead(3), 0, 1023, 256, 32768);
  OCR1A = F_CPU / SAMPLE_RATE;
}

///////////////////////////////////////////////////////////////
// Update LEDs ////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////
// For Debugging //////////////////////////////////////////////
void printValues() {
  Serial.print("program: ");
  Serial.print(programNumber);
  Serial.print("  a: ");
  Serial.print(a);
  Serial.print("  b: ");
  Serial.print(b);
  Serial.print("  c: ");
  Serial.println(c);
}

///////////////////////////////////////////////////////////////
// Interrupt Timer generates audio ////////////////////////////

const uint8_t NUM_EQUATIONS = sizeof(equations) / sizeof(equations[0]);
#define totalPrograms NUM_EQUATIONS

ISR(TIMER1_COMPA_vect) {
  if (programNumber >= NUM_EQUATIONS) programNumber = 0; // Reset to first equation if out of bounds
  value = equations[programNumber].func(t, a, b, c);
  OCR2A = value;
  t++;
}

///////////////////////////////////////////////////////////////
// Button Manager /////////////////////////////////////////////
void buttonsManager() {
  static bool upButtonLastState = HIGH;
  static bool downButtonLastState = HIGH;

  // Read current states
  bool upButtonState = digitalRead(upButtonPin);
  bool downButtonState = digitalRead(downButtonPin);

  // Detect up button release (rising edge)
  if (upButtonLastState == LOW && upButtonState == HIGH) {
    programNumber++;
    if (programNumber > totalPrograms) {
      programNumber = 1;
    }
    ledCounter();
  }

  // Detect down button release (rising edge)
  if (downButtonLastState == LOW && downButtonState == HIGH) {
    if (programNumber > 1) {
      programNumber--;
    } else {
      programNumber = totalPrograms;
    }
    ledCounter();
  }

  // Store button state for next frame
  upButtonLastState = upButtonState;
  downButtonLastState = downButtonState;
}
