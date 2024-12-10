#ifndef lab2a_h
#define lab2a_h

// Display constants
#define DISPLAY_X 70
#define DISPLAY_Y 140
#define BAR_WIDTH 100
#define TEXT_Y 180

// Tuning constants
#define MAX_FREQ 460
#define MIN_FREQ 420
#define DEFAULT_FREQ 440

//new constants
#define SAMPLES 512
#define FFT_LOG2_SAMPLES 9

enum Lab2ASignals {
    ENCODER_UP = Q_USER_SIG,
    ENCODER_DOWN,
    ENCODER_CLICK,
    BTN1_PRESS,    // Standard tuning mode
    BTN2_PRESS,    // Debug mode
    BTN3_PRESS,    // Future use
    BTN4_PRESS,    // Future use
    TIMEOUT,        // For A4 overlay timeout
	NEW_FREQ_EVENT
};

typedef struct Lab2ATag {
    QActive super;
    int currentFreq;
    int a4Visible;
    int currentMode;
    float detectedFreq;
    int centOffset;
    // Add previous state tracking
    char prevNote[10];
    int prevNoteX;
    int prevNoteWidth;
    int prevBarX;
    int prevBarWidth;
    int prevBarWasRight;  // 1 if bar was on right, 0 if on left
} Lab2A;

float baseFreq;   // Current base frequency for A4 (440Hz by default)

extern Lab2A AO_Lab2A;
void Lab2A_ctor(void);
void drawA4Overlay(int freq);
void clearA4Overlay(void);
void drawDetectedFreq(float freq, int cents);
void drawNote(float freq);

#endif
