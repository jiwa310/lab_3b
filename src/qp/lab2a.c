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
    // Draw A4 frequency text in white on black
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);
    char freqText[20];
    sprintf(freqText, "A4: %d Hz", freq);
    lcdPrint(freqText, DISPLAY_X, DISPLAY_Y - 40);
}

void clearA4Overlay(void) {
    setColor(0, 0, 0);  // Black background
    fillRect(DISPLAY_X, DISPLAY_Y - 45, DISPLAY_X + BAR_WIDTH, DISPLAY_Y - 25);
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
    Lab2A *me = &AO_Lab2A;

    // Get new note info
    char noteStr[10];
    findNoteForDisplay(freq, noteStr, sizeof(noteStr));
    setFont(BigFont);
    int noteWidth = strlen(noteStr) * cfont.x_size;
    int noteX = DISPLAY_X + (BAR_WIDTH - noteWidth)/2;
    int noteY = DISPLAY_Y - 10;

    // Calculate new bar info
    const int BAR_Y = noteY + (cfont.y_size/4);
    const int BAR_HEIGHT = cfont.y_size/2;
    const int MAX_BAR_WIDTH = BAR_WIDTH/2;

    int barWidth = 0;
    int barX = 0;
    int barIsRight = (cents > 0);

    if (cents > 0) {
        barWidth = (cents * MAX_BAR_WIDTH) / 50;
        if (barWidth > MAX_BAR_WIDTH) barWidth = MAX_BAR_WIDTH;
        barX = noteX + noteWidth + 5;
    } else if (cents < 0) {
        barWidth = (-cents * MAX_BAR_WIDTH) / 50;
        if (barWidth > MAX_BAR_WIDTH) barWidth = MAX_BAR_WIDTH;
        barX = noteX - 5 - barWidth;
    }

    // Clear previous areas if different
    setColor(0, 0, 0);  // Black for clearing

    // Clear old note area if position changed
    if (strcmp(noteStr, me->prevNote) != 0 || noteX != me->prevNoteX) {
        fillRect(me->prevNoteX, noteY,
                me->prevNoteX + me->prevNoteWidth, noteY + cfont.y_size);
    }

    // Clear old bar area if position/size changed
    if (me->prevBarWidth > 0) {
        if (me->prevBarWasRight) {
            fillRect(me->prevBarX, BAR_Y,
                    me->prevBarX + me->prevBarWidth, BAR_Y + BAR_HEIGHT);
        } else {
            fillRect(me->prevBarX, BAR_Y,
                    me->prevBarX + me->prevBarWidth, BAR_Y + BAR_HEIGHT);
        }
    }

    // Draw new note and bar
    uint8_t r, g, b;
    getColorForCents(cents, &r, &g, &b);

    // Draw new note
    setColor(r, g, b);
    setColorBg(0, 0, 0);
    lcdPrint(noteStr, noteX, noteY);

    // Draw new bar
    if (barWidth > 0) {
        setColor(r, g, b);
        fillRect(barX, BAR_Y, barX + barWidth, BAR_Y + BAR_HEIGHT);
    }

    // Clear and redraw frequency and cents text
    setColor(0, 0, 0);
    fillRect(DISPLAY_X, DISPLAY_Y + 20, DISPLAY_X + BAR_WIDTH, DISPLAY_Y + 50);

    setFont(SmallFont);
    setColor(255, 255, 255);

    char freqText[20];
    sprintf(freqText, "%.1f Hz", freq);
    lcdPrint(freqText, DISPLAY_X, DISPLAY_Y + 20);

    char centsText[20];
    sprintf(centsText, "%+d cents", cents);
    lcdPrint(centsText, DISPLAY_X, DISPLAY_Y + 35);

    // Store current state for next update
    strcpy(me->prevNote, noteStr);
    me->prevNoteX = noteX;
    me->prevNoteWidth = noteWidth;
    me->prevBarX = barX;
    me->prevBarWidth = barWidth;
    me->prevBarWasRight = barIsRight;
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
    me->baseFreq = DEFAULT_FREQ;  // Initialize base frequency
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
                me->baseFreq = me->currentFreq;  // Update base frequency
                me->a4Visible = 1;
                drawA4Overlay(me->currentFreq);
            }
            return Q_HANDLED();
        }
        case ENCODER_DOWN: {
            if (me->currentFreq > MIN_FREQ) {
                me->currentFreq--;
                me->baseFreq = me->currentFreq;  // Update base frequency
                me->a4Visible = 1;
                drawA4Overlay(me->currentFreq);
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
