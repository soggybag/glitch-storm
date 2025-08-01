#include <stdint.h>
#include <avr/interrupt.h>

typedef uint8_t (*BytebeatFunc)(uint32_t, uint8_t, uint8_t, uint8_t);

struct EquationMeta {
  BytebeatFunc func;
  uint8_t aMin, aMax;
  uint8_t bMin, bMax;
  uint8_t cMin, cMax;
};

// Define some equations for the glitch storm
// Each equation takes a time variable `t` and three parameters `a`, `b`, and `c`
// The equations are designed to produce interesting sound patterns
// The parameters `a`, `b`, and `c` can be adjusted to change the sound output

// These are pointer functions that can be used to call the equations

// Add a new equation by writing a new function using the form: 
/*
uint8_t equation_1(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return <Your equation here: t >> a << t % b * t * c; >;
}
*/
// Your equation must return an 8bit value uint8_t
// Add your equation to the array below

uint8_t equation_0(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t & ((t >> a))) + (t | ((t >> b)))) & (t >> (c + 1)) | (t >> a) & (t * (t >> b));
}

uint8_t equation_1(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  // I borke this one? 
  // if (t > 65536) t = -65536;
  // if (t > 65536) t = 0;
  // return (t >> c | a | t >> (t >> 16)) * b + ((t >> (b + 1)) & (a + 1));

  // Replacement
  return t >> a + 1 << t % b * t * c;
}

uint8_t equation_2(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return (t >> c ^ t & 37 | t + (t ^ t >> a) - t * ((t % a ? 2 : 6) & t >> b) ^ t << 1 & (t & b ? t >> 4 : t >> 10));
}

uint8_t equation_3(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return b * t >> a ^ t & (37 - c) | t + ((t ^ t >> 11)) - t * ((t >> 6 ? 2 : a) & t >> (c + b)) ^ t << 1 & (t & 6 ? t >> 4 : t >> c);
}

uint8_t equation_4(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return c * t >> 2 ^ t & (30 - b) | t + ((t ^ t >> b)) - t * ((t >> 6 ? a : c) & t >> (a)) ^ t << 1 & (t & b ? t >> 4 : t >> c);
}

uint8_t equation_5(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t >> a & t) - (t >> a) + (t >> a & t)) + (t * ((t >> c) & b));
}

uint8_t equation_6(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return t >> b & t ? t >> a : -t >> c;
}

uint8_t equation_7(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  if (t > 65536) t = -65536;
  return (t >> a | c | t >> (t >> 16)) * b + ((t >> (b + 1)));
} 

uint8_t equation_8(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t * (t >> a | t >> (a & c)) & b & t >> 8)) ^ (t & t >> c | t >> 6);
}

uint8_t equation_9(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t >> c) * 7 | (t >> a) * 8 | (t >> b) * 7) & (t >> 7);
}

uint8_t equation_10(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t >> a % (128 - b << (t >> (9 - c))))) * b * t >> (c * t << 4) * t >> 18;
}

uint8_t equation_11(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return (t * 12 & t >> a | t * b & t >> c | t * b & c / (b << 2)) - 2;
}

uint8_t equation_12(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t * (t >> a) & (b * t >> 7) & (8 * t >> c)));
}

uint8_t equation_13(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return t >> c ^ t & 1 | t + (t ^ t >> 21) - t * ((t >> 4 ? b : a) & t >> (12 - (a >> 1))) ^ t << 1 & (a & 12 ? t >> 4 : t >> 10);
}

uint8_t equation_14(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t & (4 << a)) ? ((-t * (t ^ t) | (t >> b)) >> c) : (t >> 4) | ((t & (c << b)) ? t << 1 : t));
}

uint8_t equation_15(uint32_t t, uint8_t a, uint8_t b, uint8_t c) {
  return ((t & (4 << a)) ? ((-t * (t ^ t) | (t >> b)) >> 3) : (t >> c) | ((t & (3 << b)) ? t << 1 : t));
}

// If you create new equations follow the instructions above. 
// Then add your new equation to the array below. 
// If you inlcude variables in your equation define the min and max values for vars: a, b, and c

// NOTE! The LEDs show a binary value up to 15, so more equations will not display properly! 

EquationMeta equations[] = {
  { equation_0, 0, 8, 0, 12, 3, 10 },
  { equation_1, 0, 10, 0, 14, 0, 14 },
  { equation_2, 0, 12, 4, 20, 5, 12 },
  { equation_3, 6, 30, 0, 16, 0, 10 },
  { equation_4, 0, 12, 0, 16, 0, 10 },
  { equation_5, 0, 24, 0, 22, 0, 16 },
  { equation_6, 3, 10, 0, 28, 3, 10 },
  { equation_7, 0, 10, 10, 22, 0, 8 },
  { equation_8, 0, 12, 0, 20, 0, 20 },
  { equation_9, 0, 16, 0, 86, 0, 26 },
  { equation_10, 0, 8, 0, 22, 0, 16 },
  { equation_11, 0, 16, 0, 28, 3, 10 },
  { equation_12, 0, 18, 0, 28, 3, 10 },
  { equation_13, 0, 18, 0, 28, 3, 10 },
  { equation_14, 0, 8, 0, 12, 3, 10 },
  { equation_15, 0, 8, 0, 12, 3, 10 }
};

// Array of function pointers to the equations
// This allows us to easily call the equations by their index
// For example, equations[0] will call equation_1, equations[1] will call equation_2, etc.
