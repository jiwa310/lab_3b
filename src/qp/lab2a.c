#define AO_LAB2A
#include "qpn_port.h"
#include "bsp.h"
#include "lab2a.h"
#include "lcd.h"
#include "../note.h"

#define CENTS_PER_OCTAVE 1200  // 12 semitones * 100 cents
#define SEMITONE_CENTS 100

// Forward declarations of state functions
static QState Lab2A_initial(Lab2A *me);
static QState Lab2A_on(Lab2A *me);
static QState Lab2A_standardTuning(Lab2A *me);
static QState Lab2A_debugMode(Lab2A *me);

// Global variable
Lab2A AO_Lab2A;

void drawA4Overlay(int freq) {
    const int SCREEN_WIDTH = 240;
    const int SCALE = 3;
    
    // Calculate vertical position - needs to be below the cents display
    const int freqY = 40;
    const int noteY = freqY + 50;
    const int barY = noteY + (cfont.y_size * SCALE) + 40;
    const int centsY = barY + 30;
    const int a4Y = centsY + 35;  // Position below cents text
    
    // Draw A4 frequency text in white on black
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);
    
    char freqText[20];
    sprintf(freqText, "A4: %d Hz", freq);
    
    // Center the text
    setFont(BigFont);
    int textWidth = strlen(freqText) * cfont.x_size;
    int textX = (SCREEN_WIDTH - textWidth) / 2;
    
    lcdPrint(freqText, textX, a4Y);
}

void clearA4Overlay(void) {
    const int SCREEN_WIDTH = 240;
    const int SCALE = 3;
    
    // Calculate the same vertical position
    const int freqY = 40;
    const int noteY = freqY + 50;
    const int barY = noteY + (cfont.y_size * SCALE) + 40;
    const int centsY = barY + 30;
    const int a4Y = centsY + 35;
    
    // Clear the A4 overlay area
    setColor(0, 0, 0);
    fillRect(0, a4Y - 5, SCREEN_WIDTH, a4Y + 25);
}

void getColorForCents(int cents, uint8_t* r, uint8_t* g, uint8_t* b) {
    int absCents = abs(cents);

    if (absCents <= 5) {
        // Green for very good (within 5 cents)
        *r = 0;
        *g = 255;
        *b = 0;
    }
    else if (absCents <= 15) {
        // Yellow for okay (5-15 cents)
        *r = 255;
        *g = 255;
        *b = 0;
    }
    else {
        // Red for poor (>15 cents)
        *r = 255;
        *g = 0;
        *b = 0;
    }
}

void drawDetectedFreq(float freq, int cents) {
    static float prevFreq = 0;
    static char prevNoteStr[10] = "";
    static int prevCents = 0;

    const int SCALE = 3;
    const int SCREEN_WIDTH = 240;

    // Calculate vertical positions
    const int freqY = 40;
    const int noteY = freqY + 50;
    const int barY = noteY + (cfont.y_size * SCALE) + 40;
    const int centsY = barY + 30;

    // Get note info and colors
    char noteStr[10];
    findNoteForDisplay(freq, noteStr, sizeof(noteStr), AO_Lab2A.currentFreq);
    uint8_t r, g, b;
    getColorForCents(cents, &r, &g, &b);

    // Calculate positions
    setFont(BigFont);
    int noteWidth = strlen(noteStr) * cfont.x_size * SCALE;
    int noteX = (SCREEN_WIDTH - noteWidth) / 2;

    // Update frequency display if changed
    if (freq != prevFreq) {
        setColor(0, 0, 0);  // Clear old frequency
        fillRect(0, freqY - 5, SCREEN_WIDTH, freqY + 25);

        setFont(BigFont);
        setColor(255, 255, 255);
        char freqText[20];
        sprintf(freqText, "%.1f Hz", freq);
        int freqWidth = strlen(freqText) * cfont.x_size;
        int freqX = (SCREEN_WIDTH - freqWidth) / 2;
        lcdPrint(freqText, freqX, freqY);
        prevFreq = freq;
    }

    // Update note display if changed
    if (strcmp(noteStr, prevNoteStr) != 0) {
        setColor(0, 0, 0);  // Clear old note
        fillRect(0, noteY, SCREEN_WIDTH, noteY + (cfont.y_size * SCALE));

        setColor(r, g, b);
        setColorBg(0, 0, 0);
        lcdPrintScaled(noteStr, noteX, noteY);
        strcpy(prevNoteStr, noteStr);
    }

    // Bar drawing - simplified
    const int BAR_HEIGHT = 20;
    const int MAX_BAR_WIDTH = 60;  // Maximum width for each side
    const int barCenterX = SCREEN_WIDTH / 2;

    // Clear entire bar area
    setColor(0, 0, 0);
    fillRect(barCenterX - MAX_BAR_WIDTH, barY,
             barCenterX + MAX_BAR_WIDTH, barY + BAR_HEIGHT);

    // Draw center marker
    setColor(128, 128, 128);  // Gray for center marker
    fillRect(barCenterX - 1, barY, barCenterX + 1, barY + BAR_HEIGHT);

    // Draw bar
    setColor(r, g, b);
    if (cents > 0) {
        int width = (cents * MAX_BAR_WIDTH) / 50;
        if (width > MAX_BAR_WIDTH) width = MAX_BAR_WIDTH;
        fillRect(barCenterX, barY, barCenterX + width, barY + BAR_HEIGHT);
    } else if (cents < 0) {
        int width = (-cents * MAX_BAR_WIDTH) / 50;
        if (width > MAX_BAR_WIDTH) width = MAX_BAR_WIDTH;
        fillRect(barCenterX - width, barY, barCenterX, barY + BAR_HEIGHT);
    }

    // Update cents display if changed
    if (cents != prevCents) {
        setColor(0, 0, 0);
        fillRect(0, centsY - 5, SCREEN_WIDTH, centsY + 25);

        setFont(BigFont);
        setColor(255, 255, 255);
        char centsText[20];
        sprintf(centsText, "%+d cents", cents);
        int centsWidth = strlen(centsText) * cfont.x_size;
        int centsX = (SCREEN_WIDTH - centsWidth) / 2;
        lcdPrint(centsText, centsX, centsY);
        prevCents = cents;
    }
}

void drawDebugDisplay(Lab2A *me) {
    setColor(0, 0, 0);  // Black background
    fillRect(0, 0, 240, 320);

    setColor(255, 255, 255);  // White text
    setColorBg(0, 0, 0);

    char text[30];
    int y = 20;

    sprintf(text, "Debug Display");
    lcdPrint(text, 10, y);
    y += 30;

    sprintf(text, "A4 Reference: %d Hz", me->currentFreq);
    lcdPrint(text, 10, y);
    y += 20;

    sprintf(text, "Input: %.1f Hz", me->detectedFreq);
    lcdPrint(text, 10, y);
    y += 20;

    sprintf(text, "Cents: %+d", me->centOffset);
    lcdPrint(text, 10, y);
}

void Lab2A_ctor(void) {
    Lab2A *me = &AO_Lab2A;
    me->currentFreq = DEFAULT_FREQ;
    me->a4Visible = 0;
    me->currentMode = 0;
    me->detectedFreq = 0.0f;
    me->centOffset = 0;
    QActive_ctor(&me->super, (QStateHandler)Lab2A_initial);
}

QState Lab2A_initial(Lab2A *me) {
    return Q_TRAN(Lab2A_on);
}

QState Lab2A_on(Lab2A *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            return Q_HANDLED();
        }
        case Q_INIT_SIG: {
            return Q_TRAN(Lab2A_standardTuning);
        }
    }
    return Q_SUPER(&QHsm_top);
}

QState Lab2A_standardTuning(Lab2A *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            drawDetectedFreq(me->detectedFreq, me->centOffset);
            if (me->a4Visible) {
                drawA4Overlay(me->currentFreq);
            }
            return Q_HANDLED();
        }
        case ENCODER_UP: {
            if (me->currentFreq < MAX_FREQ) {
                me->currentFreq++;
                me->a4Visible = 1;
                drawA4Overlay(me->currentFreq);
                // Recalculate cents with new base frequency
                // me->centOffset = calculateCents(me->detectedFreq, me->currentFreq);
                // drawDetectedFreq(me->detectedFreq, me->centOffset);
            }
            return Q_HANDLED();
        }
        case ENCODER_DOWN: {
            if (me->currentFreq > MIN_FREQ) {
                me->currentFreq--;
                me->a4Visible = 1;
                drawA4Overlay(me->currentFreq);
                // Recalculate cents with new base frequency
                // me->centOffset = calculateCents(me->detectedFreq, me->currentFreq);
                // drawDetectedFreq(me->detectedFreq, me->centOffset);
            }
            return Q_HANDLED();
        }
        case BTN1_PRESS: {
            me->currentFreq = DEFAULT_FREQ;
            me->a4Visible = 1;
            drawA4Overlay(me->currentFreq);
            return Q_HANDLED();
        }
        case BTN2_PRESS: {
            return Q_TRAN(Lab2A_debugMode);
        }
        case TIMEOUT: {
            if (me->a4Visible) {
                me->a4Visible = 0;
                clearA4Overlay();
            }
            return Q_HANDLED();
        }
        case NEW_FREQ_EVENT: {
			drawDetectedFreq(me->detectedFreq, me->centOffset);
			return Q_HANDLED();
		}

    }
    return Q_SUPER(Lab2A_on);
}

QState Lab2A_debugMode(Lab2A *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            drawDebugDisplay(me);
            return Q_HANDLED();
        }
        case ENCODER_UP: {
            if (me->currentFreq < MAX_FREQ) {
                me->currentFreq++;
                drawDebugDisplay(me);
            }
            return Q_HANDLED();
        }
        case ENCODER_DOWN: {
            if (me->currentFreq > MIN_FREQ) {
                me->currentFreq--;
                drawDebugDisplay(me);
            }
            return Q_HANDLED();
        }
        case BTN1_PRESS: {
            me->currentFreq = DEFAULT_FREQ;
            drawDebugDisplay(me);
            return Q_HANDLED();
        }
        case BTN2_PRESS: {
            return Q_TRAN(Lab2A_standardTuning);
        }

    }
    return Q_SUPER(Lab2A_on);
}
