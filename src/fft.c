#include "fft.h"
#include "complex.h"
#include "trig.h"
#include "globals.h"

#define MAX_FFT_SIZE 512
static float cos_table[MAX_FFT_SIZE];
static float sin_table[MAX_FFT_SIZE];

static float new_[512];
static float new_im[512];

//new

float current_frequency = 0;
int current_cents_offset = 0;

// Initialize trig tables (call once at startup)
void init_fft_tables(void) {
    int i;
    for(i = 0; i < MAX_FFT_SIZE; i++) {
        cos_table[i] = cosine(-PI * i / MAX_FFT_SIZE);
        sin_table[i] = sine(-PI * i / MAX_FFT_SIZE);
    }
}

float fft(float* q, float* w, int n, int m, float sample_f) {
    int a, b, r, d, e, c;
    int k, place;
    float real = 0, imagine = 0;
    float max, frequency;

    // Remove DC offset for better low-frequency accuracy
    float dc_offset = 0;
    for(int i = 0; i < n; i++) {
        dc_offset += q[i];
    }
    dc_offset /= n;
    for(int i = 0; i < n; i++) {
        q[i] -= dc_offset;
    }

    // Ordering algorithm (bit reversal)
    a = n/2;
    b = 1;
    for(int i = 0; i < (m-1); i++) {
        d = 0;
        for(int j = 0; j < b; j++) {
            for(c = 0; c < a; c++) {
                e = c + d;
                new_[e] = q[(c*2) + d];
                new_im[e] = w[(c*2) + d];
                new_[e + a] = q[2*c + 1 + d];
                new_im[e + a] = w[2*c + 1 + d];
            }
            d += (n/b);
        }
        // Copy back using memcpy for efficiency
        memcpy(q, new_, n * sizeof(float));
        memcpy(w, new_im, n * sizeof(float));
        b *= 2;
        a = n/(2*b);
    }

    // FFT computation with lookup tables
    b = 1;
    k = 0;
    for(int j = 0; j < m; j++) {
        // MATH section
        for(int i = 0; i < n; i += 2) {
            if(i%(n/b) == 0 && i != 0)
                k++;

            // Use pre-computed trig values
            int table_index = (k * MAX_FFT_SIZE) / b;
            float cos_val = cos_table[table_index];
            float sin_val = sin_table[table_index];

            // Combined multiplication to reduce operations
            float temp_real = q[i+1];
            float temp_imag = w[i+1];
            real = temp_real * cos_val - temp_imag * sin_val;
            imagine = temp_real * sin_val + temp_imag * cos_val;

            new_[i] = q[i] + real;
            new_im[i] = w[i] + imagine;
            new_[i+1] = q[i] - real;
            new_im[i+1] = w[i] - imagine;
        }

        // Copy results using memcpy
        memcpy(q, new_, n * sizeof(float));
        memcpy(w, new_im, n * sizeof(float));

        // REORDER section
        for(int i = 0; i < n/2; i++) {
            new_[i] = q[2*i];
            new_[i + (n/2)] = q[2*i + 1];
            new_im[i] = w[2*i];
            new_im[i + (n/2)] = w[2*i + 1];
        }

        // Copy back using memcpy
        memcpy(q, new_, n * sizeof(float));
        memcpy(w, new_im, n * sizeof(float));

        b *= 2;
        k = 0;
    }

    // Find magnitudes with combined loop
    max = 0;
    place = 1;
    float bin_spacing = sample_f/n;  // Pre-calculate bin spacing

    for(int i = 1; i < (n/2); i++) {
        new_[i] = q[i]*q[i] + w[i]*w[i];  // Store magnitude squared
        if(max < new_[i]) {
            max = new_[i];
            place = i;
        }
    }

    // Parabolic interpolation for better accuracy
    float y1 = new_[place-1];
    float y2 = new_[place];
    float y3 = new_[place+1];

    // Combined calculation to reduce divisions
    float x0 = bin_spacing + (2.0f * bin_spacing * (y2-y1))/(2*y2-y1-y3);
    x0 = x0/bin_spacing - 1.0f;

    if(x0 < 0 || x0 > 2) {
        return 0;
    }

    // Calculate final frequency with interpolation
    frequency = bin_spacing * place;
    frequency += (x0 <= 1) ? (-(1-x0) * bin_spacing) : ((x0-1) * bin_spacing);

    return frequency;
}
