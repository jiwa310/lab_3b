#include "fft.h"
#include "complex.h"
#include "trig.h"
#include <string.h>

// Global lookup tables
float cosLUT[9][256];
float sinLUT[9][256];

// Static buffers for FFT computation
static float new_[MAX_FFT_SIZE];
static float new_im[MAX_FFT_SIZE];

void LUT_construct(void) {
    int b = 1;
    int k = 0;

    for(int j = 0; j < 9; j++) {
        for(int i = 0; i < 512; i += 2) {
            if(i%(512/b) == 0 && i != 0)
                k++;
            // functions in trig.c
            cosLUT[j][k] = cos(-PI*k/b);
            sinLUT[j][k] = sin(-PI*k/b);
        }
        b *= 2;
        k = 0;
    }
}

float fft(float* q, float* w, int n, int m, float sample_f) {
    int a = n/2;
    int b = 1;
    float real = 0, imagine = 0;
    float max, frequency;

    // Remove DC offset
    float dc_offset = 0;
    for(int i = 0; i < n; i++) {
        dc_offset += q[i];
    }
    dc_offset /= n;
    for(int i = 0; i < n; i++) {
        q[i] -= dc_offset;
    }

    // Ordering algorithm (bit reversal)
    for(int i = 0; i < (m-1); i++) {
        int d = 0;
        for(int j = 0; j < b; j++) {
            for(int c = 0; c < a; c++) {
                int e = c + d;
                new_[e] = q[(c*2) + d];
                new_im[e] = w[(c*2) + d];
                new_[e + a] = q[2*c + 1 + d];
                new_im[e + a] = w[2*c + 1 + d];
            }
            d += (n/b);
        }
        memcpy(q, new_, n * sizeof(float));
        memcpy(w, new_im, n * sizeof(float));
        b *= 2;
        a = n/(2*b);
    }

    // FFT computation
    b = 1;
    int k = 0;
    for(int j = 0; j < m; j++) {
        // Math section
        for(int i = 0; i < n; i += 2) {
            if(i%(n/b) == 0 && i != 0) {
                k++;
            }
            real = mult_real(q[i+1], w[i+1], cosLUT[j][k], sinLUT[j][k]);
            imagine = mult_im(q[i+1], w[i+1], cosLUT[j][k], sinLUT[j][k]);
            new_[i] = q[i] + real;
            new_im[i] = w[i] + imagine;
            new_[i+1] = q[i] - real;
            new_im[i+1] = w[i] - imagine;
        }
        memcpy(q, new_, n * sizeof(float));
        memcpy(w, new_im, n * sizeof(float));

        // Reorder section
        for(int i = 0; i < n/2; i++) {
            new_[i] = q[2*i];
            new_[i + (n/2)] = q[2*i + 1];
            new_im[i] = w[2*i];
            new_im[i + (n/2)] = w[2*i + 1];
        }
        memcpy(q, new_, n * sizeof(float));
        memcpy(w, new_im, n * sizeof(float));

        b *= 2;
        k = 0;
    }

    // Find maximum magnitude
    max = 0;
    int place = 1;
    for(int i = 1; i < (n/2); i++) {
        new_[i] = q[i]*q[i] + w[i]*w[i];  // Store magnitude squared
        if(max < new_[i]) {
            max = new_[i];
            place = i;
        }
    }

    float s = sample_f/n;  // Bin spacing
    frequency = (sample_f/n) * place;

    // Parabolic interpolation for better accuracy
    if(place > 0 && place < (n/2)-1) {
        float y1 = new_[place-1];
        float y2 = new_[place];
        float y3 = new_[place+1];

        // Calculate peak offset using quadratic interpolation
        float x0 = s + (2*s*(y2-y1))/(2*y2-y1-y3);
        x0 = x0/s - 1;

        // Apply correction if within valid range
        if(x0 >= 0 && x0 <= 2) {
            if(x0 <= 1) {
                frequency = frequency - (1-x0)*s;
            } else {
                frequency = frequency + (x0-1)*s;
            }
        }
    }

    return frequency;
}
