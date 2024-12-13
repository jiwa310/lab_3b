#ifndef GLOBALS_H
#define GLOBALS_H

#define SAMPLES 512
#define FFT_LOG2_SAMPLES 9  // 2^9 = 512
#define M 9

// Existing declarations
extern float q[SAMPLES];
extern float w[SAMPLES];
extern int int_buffer[SAMPLES];

// Add these new declarations
extern volatile int dirFlag;
extern float sample_f;
extern int l;
extern float frequency;
extern int factor;
extern int octave;

#endif
