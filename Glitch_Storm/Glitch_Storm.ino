// Glitch Storm
// CC By Sa Spherical Sound Society 2020
// Heavy inspiration in Bytebeat (Viznut)
// Some equations are empty. You can collaborate sending your new finding cool sounding ones to the repository
// https://github.com/spherical-sound-society/glitch-storm

// This is my take on Glitch Storm. I revised the original files and made a few changes to suit 
// my software design style.

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "equations.h"

#define isDebugging   true // Set to true to enable serial output for debugging

#define ledPin        13 // On-board LED
#define speakerPin    11 // PWM pin for audio output
#define upButtonPin   2  // Pin for the "up" button
#define downButtonPin 4  // Pin for the "down" button

// Pins for the 4 LEDs showing the current program number in binary
#define progBit0Pin   7  // Pin for the "program bit 0"
#define progBit1Pin   8  // Pin for the "program bit 1"
#define progBit2Pin   9  // Pin for the "program bit 2"
#define progBit3Pin   10 // Pin for the "program bit 3"

long t = 0;                           // time variable for equations
volatile uint8_t a = 0, b = 0, c = 0; // parameters for equations
volatile int value;                   // current output value

byte programNumber = 0;   // current program number
byte upButtonState = 0;   // state of the "up" button
byte downButtonState = 0; // state of the "down" button
byte clocksOut = 0;       // number of clock outputs

static unsigned long time_now = 0; // For debugging

int SAMPLE_RATE = 16384; // Initial sample rate


///////////////////////////////////////////////////////////////
// SETUP AUDIO OUTPUT /////////////////////////////////////////

// Initialize sound output
// Note: this function sets up timer 1 and 2 and nothing here should be modified. 
// Read about timers here: https://docs.arduino.cc/tutorials/generic/secrets-of-arduino-pwm/
void initSound() {
  pinMode(speakerPin, OUTPUT);       // Set speaker pin as output

  // Timer 2 generates audio on PWM pin OC2A (Arduino pin 11)
  // ASSR -> Async Shift Register, TCCR2A / TCCR2B: Timer/Counter Control Registers A and B
  // WGMn -> Waveform Generation Mode n
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));  // Use internal clock for Timer 2 (p.154) _BV() bit value
  TCCR2A |= _BV(WGM21) | _BV(WGM20); // Set Fast PWM mode (p.155)
  TCCR2B &= ~_BV(WGM22);             // (part of WGM22 is in TCCR2B)

  // Do non-inverting PWM on pin OC2A (p.155)
  // On the Arduino this is pin 11.
  // TCCRnA -> Timer/Counter Control Register nA
  // COMnA -> Compare Output Mode nA
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0); // Clear OC2A on Compare Match, set at BOTTOM (non-inverting mode)
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0)); // Disconnect OC2B
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // Set CS22, CS21, CS20 bits for no prescaling

  // Set initial pulse width to the first sample.
  // OCRnA -> Output Compare Register nA
  OCR2A = 0; // Output Compare Register 2A (8-bit, p.156)

  // Set up Timer 1 to send a sample every interrupt.
  cli(); // Disable interrupts while we set up

  // Timer 1 is a 16-bit timer (p.132)
  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12); // Set WGM13:0 to 0100
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10)); // (part of WGM13:0 is in TCCR1A)

  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // Set CS12, CS11, CS10 bits for no prescaling

  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, we have to do this with
  // interrupts disabled to be safe.
  OCR1A = F_CPU / SAMPLE_RATE; // 16e6 / 8000 = 2000
  // Timer 1 calls Interrupt Service Routine ISR(TIMER1_COMPA_vect) at SAMPLE_RATE

  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A); // Enable Timer1 Compare A Match interrupt

  sei(); // Re-enable interrupts

  // Timer 2 handles audio output via PWM on pin 11
  // Timer 1 runs bytebeat calculations with an interrupt at the sample rate
}

///////////////////////////////////////////////////////////////
// SETUP! /////////////////////////////////////////////////////

// Initialize pins and serial, and initialize audio output
void setup() {
  pinMode(ledPin, OUTPUT);      // On-board LED
  pinMode(progBit0Pin, OUTPUT); // Program bit 0
  pinMode(progBit1Pin, OUTPUT); // Program bit 1
  pinMode(progBit2Pin, OUTPUT); // Program bit 2
  pinMode(progBit3Pin, OUTPUT); // Program bit 3

  pinMode(upButtonPin, INPUT_PULLUP);   // Use internal pull-up resistor
  pinMode(downButtonPin, INPUT_PULLUP); // Use internal pull-up resistor

  initSound();  // Initialize audio output
  ledCounter(); // Update LEDs to show initial program number

  // Initialize serial for debugging
  if (isDebugging) {
    Serial.begin(9600);
  }
}

///////////////////////////////////////////////////////////////
// LOOP! //////////////////////////////////////////////////////

// Main loop
// Note: audio is handled in the interrupt service routine (ISR(TIMER1_COMPA_vect) below)
void loop() {
  buttonsManager(); // Check buttons and update program number
  potsManager();    // Read pots and update a, b, c, and sample rate

  // slow loop show serial once every second
  if (isDebugging) {
    if (millis() > time_now + 1000) {
      time_now = millis();
      printValues();
    }
  }
}

///////////////////////////////////////////////////////////////
// Handle Pots ////////////////////////////////////////////////

// Helper function to scale parameters
// Scale analog input (0-1023) to a value between minVal and maxVal
// returns a uint 8 (0-255)
uint8_t scaleParam(int analogValue, uint8_t minVal, uint8_t maxVal) {
  return map(analogValue, 0, 1023, minVal, maxVal);
}

// Read pots and update a, b, c, and sample rate
void potsManager() {
  // Get Pots for vars a, b, and c
  int analogA = analogRead(A0); // A0 pin 19
  int analogB = analogRead(A1); // A1 pin 20
  int analogC = analogRead(A2); // A2 pin 21

  // Scale vars a, b, and c, by the ranges for each equation
  a = scaleParam(analogA, equations[programNumber].aMin, equations[programNumber].aMax);
  b = scaleParam(analogB, equations[programNumber].bMin, equations[programNumber].bMax);
  c = scaleParam(analogC, equations[programNumber].cMin, equations[programNumber].cMax);

  // Sample rate uses a unit 16, can't use scaleParam here! 
  SAMPLE_RATE = map(analogRead(A3), 0, 1023, 256, 32768);
 
  OCR1A = F_CPU / SAMPLE_RATE; // Update Timer1 compare register
}

///////////////////////////////////////////////////////////////
// Update LEDs ////////////////////////////////////////////////

// Helper function to update the 4 LEDs
// Update the 4 LEDs to show the current program number in binary
void ledCounter() {
  digitalWrite(progBit0Pin, bitRead(programNumber, 0));
  digitalWrite(progBit1Pin, bitRead(programNumber, 1));
  digitalWrite(progBit2Pin, bitRead(programNumber, 2));
  digitalWrite(progBit3Pin, bitRead(programNumber, 3));
}

///////////////////////////////////////////////////////////////
// For Debugging //////////////////////////////////////////////

// Helper function for debugging. 
// Print current program number and parameters a, b, c to serial
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

// Get the Number of equations
const uint8_t NUM_EQUATIONS = sizeof(equations) / sizeof(equations[0]);
#define totalPrograms NUM_EQUATIONS

// Timer1 Compare A interrupt service routine
// This generates audio samples from bytebeat equations
ISR(TIMER1_COMPA_vect) {
  if (programNumber >= NUM_EQUATIONS) programNumber = 0; // Reset to first equation if out of bounds
  value = equations[programNumber].func(t, a, b, c); // Compute next sample value
  OCR2A = value; // Update PWM duty cycle for audio output
  t++; // Increment time variable
}

///////////////////////////////////////////////////////////////
// Button Manager /////////////////////////////////////////////

// Helper function to handle button presses to change program number
void buttonsManager() {
  static bool upButtonLastState = HIGH;   // Buttons are active LOW
  static bool downButtonLastState = HIGH; // Buttons are active LOW

  // Read current states
  bool upButtonState = digitalRead(upButtonPin);     // Read "up" button state
  bool downButtonState = digitalRead(downButtonPin); // Read "down" button state

  // Detect up button (D2 pin 5) release (rising edge)
  if (upButtonLastState == LOW && upButtonState == HIGH) {
    programNumber++; // Increment program number
    // Wrap around if exceeding total programs
    if (programNumber > totalPrograms) { 
      programNumber = 0; 
    }
    ledCounter(); // Update LEDs to show new program number
  }

  // Detect down button (D4 pin 7) release (rising edge)
  if (downButtonLastState == LOW && downButtonState == HIGH) {
    // Decrement program number with wrap-around
    if (programNumber > 0) {
      programNumber--; 
    } else {
      programNumber = totalPrograms - 1; // Wrap around to last program
    }
    ledCounter(); // Update LEDs to show new program number
  }

  // Store button state for next frame
  upButtonLastState = upButtonState;     // Store "up" button state
  downButtonLastState = downButtonState; // Store "down" button state
}
