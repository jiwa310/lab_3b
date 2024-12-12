#ifndef FFT_H
#define FFT_H

#include "globals.h"

#define PI 3.141592
#define MAX_FFT_SIZE 512

// Lookup tables for sine and cosine values
extern float cosLUT[9][256];
extern float sinLUT[9][256];

// Lookup table construction function
void LUT_construct(void);

// Main FFT function
float fft(float* q, float* w, int n, int m, float sample_f);

#endif
