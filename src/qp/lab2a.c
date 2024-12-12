#define AO_LAB2A
#include <stdio.h>
#include <stdlib.h>
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
    Lab2A *me = &AO_Lab2A;

    // Constants that don't depend on calculations
    static const int SCALE = 3;
    static const int SCREEN_WIDTH = 240;
    static const int freqY = 40;
    static const int noteY = freqY + 50;
    static const int BAR_HEIGHT = 20;
    static const int MAX_BAR_WIDTH = 60;

    // Set font first to ensure consistent calculations
    setFont(BigFont);

    // Calculate dependent positions
    const int barY = noteY + (cfont.y_size * SCALE) + 40;
    const int centsY = barY + 30;
    const int barCenterX = SCREEN_WIDTH / 2;

    // Get note info and colors
    char noteStr[10];
    findNoteForDisplay(freq, noteStr, sizeof(noteStr), me->currentFreq);
    const int noteWidth = strlen(noteStr) * cfont.x_size * SCALE;
    const int noteX = (SCREEN_WIDTH - noteWidth) / 2;

    uint8_t r, g, b;
    getColorForCents(cents, &r, &g, &b);

    // Update frequency display if changed
    if (freq != me->prevFreq) {
        setColor(0, 0, 0);  // Clear old frequency
        fillRect(0, freqY - 5, SCREEN_WIDTH, freqY + 25);

        setFont(BigFont);
        setColor(255, 255, 255);
        char freqText[20];
        sprintf(freqText, "%.1f Hz", freq);
        int freqWidth = strlen(freqText) * cfont.x_size;
        int freqX = (SCREEN_WIDTH - freqWidth) / 2;
        lcdPrint(freqText, freqX, freqY);
        me->prevFreq = freq;
    }

    // Update note display if changed
    if (strcmp(noteStr, me->prevNoteStr) != 0) {
        setColor(0, 0, 0);  // Clear old note
        fillRect(0, noteY, SCREEN_WIDTH, noteY + (cfont.y_size * SCALE));

        setColor(r, g, b);
        setColorBg(0, 0, 0);
        lcdPrintScaled(noteStr, noteX, noteY);
        strcpy(me->prevNoteStr, noteStr);
    }

    // Bar drawing - only update if needed
    int width = 0;
    int barSide = 0;
    if (cents > 0) {
        width = (cents * MAX_BAR_WIDTH) / 50;
        if (width > MAX_BAR_WIDTH) width = MAX_BAR_WIDTH;
        barSide = 1;
    } else if (cents < 0) {
        width = (-cents * MAX_BAR_WIDTH) / 50;
        if (width > MAX_BAR_WIDTH) width = MAX_BAR_WIDTH;
        barSide = -1;
    }

    // Only update bar if position or color changed
    if (width != me->prevBarWidth || barSide != me->prevBarSide ||
        r != me->prevR || g != me->prevG || b != me->prevB) {
        
        // Clear entire bar area with some padding
        setColor(0, 0, 0);
        fillRect(barCenterX - MAX_BAR_WIDTH - 2, barY - 2,
                barCenterX + MAX_BAR_WIDTH + 2, barY + BAR_HEIGHT + 2);

        // Draw center marker
        setColor(128, 128, 128);
        fillRect(barCenterX - 1, barY, barCenterX + 1, barY + BAR_HEIGHT);

        // Draw new bar
        setColor(r, g, b);
        if (barSide > 0) {
            fillRect(barCenterX, barY, barCenterX + width, barY + BAR_HEIGHT);
        } else if (barSide < 0) {
            fillRect(barCenterX - width, barY, barCenterX, barY + BAR_HEIGHT);
        }

        me->prevBarWidth = width;
        me->prevBarSide = barSide;
        me->prevR = r;
        me->prevG = g;
        me->prevB = b;
    }

    // Update cents display if changed
    if (cents != me->prevCents) {
        setColor(0, 0, 0);
        fillRect(0, centsY - 5, SCREEN_WIDTH, centsY + 25);

        setFont(BigFont);
        setColor(255, 255, 255);
        char centsText[20];
        sprintf(centsText, "%+d cents", cents);
        int centsWidth = strlen(centsText) * cfont.x_size;
        int centsX = (SCREEN_WIDTH - centsWidth) / 2;
        lcdPrint(centsText, centsX, centsY);
        me->prevCents = cents;
    }
}

void drawDebugDisplay(Lab2A *me) {
    // Clear screen
    setColor(0, 0, 0);
    fillRect(0, 0, 240, 320);

    // Header section
    setColor(255, 255, 255);
    setFont(SmallFont);
    char text[30];
    int y = 10;

    // Display current settings and detected frequency
    sprintf(text, "A4: %d Hz", me->currentFreq);
    lcdPrint(text, 10, y);
    y += 15;

    sprintf(text, "Detected: %.1f Hz", me->detectedFreq);
    lcdPrint(text, 10, y);
    y += 15;

    sprintf(text, "Offset: %+d cents", me->centOffset);
    lcdPrint(text, 10, y);
    y += 25;

    // FFT Display section
    setColor(255, 255, 255);
    lcdPrint("FFT Spectrum:", 10, y);
    y += 20;

    // Draw axes
    int graphX = 30;        // Left margin
    int graphY = y;         // Top of graph
    int graphW = 180;       // Graph width
    int graphH = 120;       // Graph height

    // Draw axes
    drawHLine(graphX, graphY + graphH, graphW);  // X axis
    for(int i = 0; i < graphH; i += 20) {        // Y axis ticks
        drawHLine(graphX - 2, graphY + i, 4);
    }

    // Plot FFT bins around detected frequency
    if(me->detectedFreq > 0) {
        float bin_spacing = (SAMPLE_RATE/4) / SAMPLES;  // Assuming 4x averaging
        int center_bin = (int)(me->detectedFreq / bin_spacing);
        int bins_to_show = 15;  // Show 15 bins on each side

        // Draw FFT bars
        setColor(0, 255, 0);  // Green for FFT data
        for(int i = -bins_to_show; i <= bins_to_show; i++) {
            int bin = center_bin + i;
            if(bin >= 0 && bin < SAMPLES/2) {
                // Get magnitude from q and w arrays (stored in globals)
                float mag = q[bin]*q[bin] + w[bin]*w[bin];
                // Log scale for better visibility
                mag = 10 * log10f(mag + 1);  // +1 to avoid log(0)

                // Scale to fit graph
                int height = (int)((mag * graphH) / 100);
                if(height > graphH) height = graphH;
                if(height < 0) height = 0;

                // Draw bar
                int x = graphX + (i + bins_to_show) * (graphW/(2*bins_to_show));
                fillRect(x, graphY + graphH - height, x + 2, graphY + graphH);
            }
        }

        // Mark detected frequency bin
        setColor(255, 0, 0);  // Red marker
        int x = graphX + bins_to_show * (graphW/(2*bins_to_show));
        fillRect(x-1, graphY, x+1, graphY + graphH);
    }

    // Instructions at bottom
    y = 280;
    setColor(0, 255, 0);
    lcdPrint("BTN3: Return to Tuner", 10, y);
}

void Lab2A_ctor(void) {
    Lab2A *me = &AO_Lab2A;
    me->currentFreq = DEFAULT_FREQ;
    me->a4Visible = 0;
    me->currentMode = 0;
    me->detectedFreq = 0.0f;
    me->centOffset = 0;
    me->historyIndex = 0;
    
    // Initialize display state tracking
    me->prevFreq = 0.0f;
    me->prevNoteStr[0] = '\0';
    me->prevCents = 0;
    me->prevR = me->prevG = me->prevB = 0;
    me->prevBarWidth = 0;
    me->prevBarSide = 0;

    // Clear history
    for (int i = 0; i < FREQ_HISTORY_SIZE; i++) {
        me->freqHistory[i].freq = 0.0f;
        me->freqHistory[i].cents = 0;
        me->freqHistory[i].note[0] = '\0';
    }
    
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
			xil_printf("Entering standard tuning mode\n\r");
			me->currentMode = 0;

			// Reset state tracking variables to force redraw
			me->prevFreq = 0.0f;
			me->prevNoteStr[0] = '\0';
			me->prevCents = 0;
			me->prevR = me->prevG = me->prevB = 0;
			me->prevBarWidth = 0;
			me->prevBarSide = 0;

			// Force complete redraw of display
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
            }
            return Q_HANDLED();
        }
        case ENCODER_DOWN: {
            if (me->currentFreq > MIN_FREQ) {
                me->currentFreq--;
                me->a4Visible = 1;
                drawA4Overlay(me->currentFreq);
            }
            return Q_HANDLED();
        }
        case BTN2_PRESS: {
            xil_printf("BTN2 handled in standard mode - transitioning to debug\n\r");
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
            // Store in history
            me->freqHistory[me->historyIndex].freq = me->detectedFreq;
            me->freqHistory[me->historyIndex].cents = me->centOffset;
            findNoteForDisplay(me->detectedFreq, me->freqHistory[me->historyIndex].note,
                            sizeof(me->freqHistory[me->historyIndex].note),
                            me->currentFreq);

            // Advance history index (circular buffer)
            me->historyIndex = (me->historyIndex + 1) % FREQ_HISTORY_SIZE;

            // Always draw when receiving new frequency data
            drawDetectedFreq(me->detectedFreq, me->centOffset);
            return Q_HANDLED();
        }
    }
    return Q_SUPER(Lab2A_on);
}

QState Lab2A_debugMode(Lab2A *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
        	xil_printf("Entering debug mode\n\r");
            me->currentMode = 1; // Set debug mode
            drawDebugDisplay(me);
            return Q_HANDLED();
        }

        case BTN3_PRESS: {      // STANDARD MODE
        	xil_printf("BTN3 handled in debug mode - Returning to standard mode\n\r");
            setColor(0, 0, 0);  // Black background
            fillRect(0, 0, 240, 320);
            return Q_TRAN(Lab2A_standardTuning);
        }

    }
    return Q_SUPER(Lab2A_on);
}
