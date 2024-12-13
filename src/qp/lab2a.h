#ifndef lab2a_h
#define lab2a_h

// Display constants
#define DISPLAY_X 70
#define DISPLAY_Y 140
#define BAR_WIDTH 120
#define TEXT_Y 180

// Tuning constants
#define MAX_FREQ 460
#define MIN_FREQ 420
#define DEFAULT_FREQ 440

//new constants
#define SAMPLES 512
#define FFT_LOG2_SAMPLES 9

#define FREQ_HISTORY_SIZE 10

enum Lab2ASignals {
    ENCODER_UP = Q_USER_SIG,
    ENCODER_DOWN,
    ENCODER_CLICK,
    BTN1_PRESS,    // Future use
    BTN2_PRESS,    // Debug mode
    BTN3_PRESS,    // Standard mode
    BTN4_PRESS,    // Future use
    TIMEOUT,        // For A4 overlay timeout
	NEW_FREQ_EVENT
};

typedef struct Lab2ATag {
    QActive super;
    int currentFreq;
    int a4Visible;
    float detectedFreq;
    int centOffset;
    int currentMode;  // 0 for standard tuning, 1 for debug mode

    // Display state tracking
    float prevFreq;
    char prevNoteStr[10];
    int prevCents;
    uint8_t prevR, prevG, prevB;
    int prevBarWidth;
    int prevBarSide;  // 0 for center, -1 for left, 1 for right

    struct {
        float freq;
        int cents;
        char note[10];
    } freqHistory[FREQ_HISTORY_SIZE];
    int historyIndex;
} Lab2A;

extern Lab2A AO_Lab2A;
void Lab2A_ctor(void);
void drawA4Overlay(int freq);
void clearA4Overlay(void);
void drawDetectedFreq(float freq, int cents);
void drawNote(float freq);

int findOctave(float freq, float baseA4);

#endif
