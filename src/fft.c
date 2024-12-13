#include "fft.h"
#include "complex.h"
#include "trig.h"
#include <string.h>
#include <math.h>
#include "xil_printf.h"

// Define the lookup tables
float cosLUT[9][256];
float sinLUT[9][256];

static float new_[512];
static float new_im[512];

void LUT_construct(void) {
    int b = 1;
    int k = 0;

    for(int j = 0; j < 9; j++) {
        for(int i = 0; i < 512; i += 2) {
            if(i%(512/b) == 0 && i != 0)
                k++;
            cosLUT[j][k] = cos(-PI*k/b);
            sinLUT[j][k] = sin(-PI*k/b);
        }
        b *= 2;
        k = 0;
    }
}

__attribute__((section(".text.fft_code")))
float fft(float* q, float* w, int n, int m, float sample_f) {
    int a = n/2;
    int b = 1;
    int k = 0;
    int place;
    float real = 0, imagine = 0;
    float max, frequency;
    int i, j, r, d, e, c;

    // Ordering algorithm
    for(i = 0; i < (m-1); i++) {
        d = 0;
        for(j = 0; j < b; j++) {
            for(c = 0; c < a; c++) {
                e = c + d;
                new_[e] = q[(c*2) + d];
                new_im[e] = w[(c*2) + d];
                new_[e + a] = q[2*c + 1 + d];
                new_im[e + a] = w[2*c + 1 + d];
            }
            d += (n/b);
        }
        for(r = 0; r < n; r++) {
            q[r] = new_[r];
            w[r] = new_im[r];
        }
        b *= 2;
        a = n/(2*b);
    }

    b = 1;
    k = 0;
    for(j = 0; j < m; j++) {
        // MATH
        for(i = 0; i < n; i += 2) {
            if(i%(n/b) == 0 && i != 0)
                k++;
            real = mult_real(q[i+1], w[i+1], cosLUT[j][k], sinLUT[j][k]);
            imagine = mult_im(q[i+1], w[i+1], cosLUT[j][k], sinLUT[j][k]);
            new_[i] = q[i] + real;
            new_im[i] = w[i] + imagine;
            new_[i+1] = q[i] - real;
            new_im[i+1] = w[i] - imagine;
        }
        for(i = 0; i < n; i++) {
            q[i] = new_[i];
            w[i] = new_im[i];
        }

        // REORDER
        for(i = 0; i < n/2; i++) {
            new_[i] = q[2*i];
            new_[i + (n/2)] = q[2*i + 1];
            new_im[i] = w[2*i];
            new_im[i + (n/2)] = w[2*i + 1];
        }
        for(i = 0; i < n; i++) {
            q[i] = new_[i];
            w[i] = new_im[i];
        }
        b *= 2;
        k = 0;
    }

    // Find magnitudes
    max = 0;
    place = 1;
    for(i = 1; i < (n/2); i++) {
        new_[i] = q[i]*q[i] + w[i]*w[i];
        if(max < new_[i]) {
            max = new_[i];
            place = i;
        }
    }

    float s = sample_f/n; // spacing of bins
    frequency = (sample_f/n)*place;

    // Debug output for peak detection
    xil_printf("\r\n------------FFT Debug------------\r\n");
    xil_printf("Peak bin: %d\r\n", place);
    xil_printf("Initial frequency estimate: %.2f Hz\r\n", frequency);

    // Curve fitting for more accuracy
    float y1 = new_[place-1];
    float y2 = new_[place];
    float y3 = new_[place+1];

    xil_printf("y1 (prev bin): %.2f\r\n", y1);
    xil_printf("y2 (peak bin): %.2f\r\n", y2);
    xil_printf("y3 (next bin): %.2f\r\n", y3);

    float x0 = s + (2*s*(y2-y1))/(2*y2-y1-y3);
    x0 = x0/s - 1;

    xil_printf("Interpolation x0: %.4f\r\n", x0);

    if(x0 < 0 || x0 > 2) { //error
        xil_printf("Interpolation error - x0 out of range\r\n");
        return 0;
    }

    float freq_adjustment;
    if(x0 <= 1) {
        freq_adjustment = -(1-x0)*s;
        frequency += freq_adjustment;
        xil_printf("Negative adjustment: %.2f Hz\r\n", freq_adjustment);
    } else {
        freq_adjustment = (x0-1)*s;
        frequency += freq_adjustment;
        xil_printf("Positive adjustment: %.2f Hz\r\n", freq_adjustment);
    }

    xil_printf("Final frequency: %.2f Hz\r\n", frequency);
    xil_printf("--------------------------------\r\n");

    return frequency;
}
