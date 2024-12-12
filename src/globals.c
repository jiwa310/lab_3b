#include "globals.h"

// Existing definitions
float q[SAMPLES] = {0};
float w[SAMPLES] = {0};
int int_buffer[SAMPLES] = {0};

// Add these new definitions
float sample_f = 0;
int l = 0;
float frequency = 0;
int factor = 1;
int octave = 4;  // Starting at middle octave
