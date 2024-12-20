/*****************************************************************************
* bsp.c for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 27,2019
*****************************************************************************/

/**/
#include "qpn_port.h"
#include "bsp.h"
#include "lab2a.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xspi.h"
#include "xspi_l.h"
#include "lcd.h"
#include "xtmrctr.h"
#include "xtmrctr_l.h"
#include "../globals.h"

#include "../fft.h"
#include "../stream_grabber.h"
#include <string.h>

/*****************************/

/* Define all variables and Gpio objects here  */
// Device instances
static XGpio dc;          // For LCD DC (Data/Command) signal
static XSpi spi;          // For LCD SPI interface
static XTmrCtr axiTimer;  // For 2-second inactivity timer
static XIntc intc;        // Interrupt controller

static XGpio encoder;    // For rotary encoder
static XGpio btnGpio;  // For push buttons
#define S0 0
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define S5 5
#define S6 6
#define DEBOUNCE_TICKS 100

#define MAX(a,b) ((a) > (b) ? (a) : (b))

// State variables for encoder
volatile int currentState = S0;
volatile int directionFlag = 0;
volatile int onFlag = 1;
volatile int button_press_pending = 0;
volatile unsigned int last_button_press_time = 0;

// Forward declaration of interrupt handler
void encoder_handler(void *CallbackRef);
void button_handler(void *CallbackRef);

// Timer count variable
volatile unsigned int count = 0;

// State variables
volatile int lastActivityTime = 0;

#define GPIO_CHANNEL1 1

void debounceInterrupt(); // Write This function

int calculateCents(float f, float nearestNoteFreq) {
    // Validate input frequencies
    if (f <= 0 || nearestNoteFreq <= 0) {
        return 0;
    }

    // Calculate next note up (r in friend's code)
    float nextNoteFreq = nearestNoteFreq * pow(2.0, 1.0/12.0);

    // Use his exact calculation
    if ((f - nearestNoteFreq) <= (nextNoteFreq - f)) {  //closer to left note
        return (int)((f - nearestNoteFreq)/((nextNoteFreq - nearestNoteFreq)/100));
    } else {  //closer to right note
        return (int)((f - nextNoteFreq)/((nextNoteFreq - nearestNoteFreq)/100));
    }
}

// Get the nearest note frequency
float getNearestNoteFreq(float freq, float a4Freq) {
    if (freq <= 0) return a4Freq;

    // Scale all note frequencies based on A4 tuning
    float scalingFactor = a4Freq / 440.0;  // How much to scale standard frequencies

    // Find the nearest note in the standard scale, then apply the scaling
    float normalizedFreq = freq / scalingFactor;  // Convert to standard A440 scale
    float a4 = 440.0;  // A4 in standard tuning
    float semitones = 12.0 * log2(normalizedFreq/a4);
    int nearestSemitone = (int)(semitones + 0.5);

    // Calculate the note frequency in A440 scale, then scale to the current A4 reference
    float standardFreq = a4 * pow(2.0, nearestSemitone/12.0);
    return standardFreq * scalingFactor;
}

// Create ONE interrupt controllers XIntc
// Create two static XGpio variables
// Suggest Creating two int's to use for determining the direction of twist

void TimerCounterHandler(void *CallBackRef, u8 TmrCtrNumber) {
    count++;  // Increment general timer count
    lastActivityTime++;  // Track time since last activity

    // Post timeout event if 2 seconds of inactivity
    if (lastActivityTime == 200000) {  // Assuming timer ticks every 10ms
        QActive_postISR((QActive *)&AO_Lab2A, TIMEOUT);
        // Don't need to reset lastActivityTime here since we want
        // the volume bar to stay hidden until new activity
        xil_printf("TIMEOUT\n\r");
    }

    // This is the interrupt handler function
	// Do not print inside of this function.
	Xuint32 ControlStatusReg;
	/*
	 * Read the new Control/Status Register content.
	 */
	ControlStatusReg =
			XTimerCtr_ReadReg(axiTimer.BaseAddress, 0, XTC_TCSR_OFFSET);

	// xil_printf("Timer interrupt occurred. Count= %d\r\n", count);
	// XGpio_DiscreteWrite(&led,1,count);
	count++;			// increment count
	/*
	 * Acknowledge the interrupt by clearing the interrupt
	 * bit in the timer control status register
	 */
	XTmrCtr_WriteReg(axiTimer.BaseAddress, 0, XTC_TCSR_OFFSET,
			ControlStatusReg |XTC_CSR_INT_OCCURED_MASK);

}


/*..........................................................................*/
void BSP_init(void) {
    u32 status;
    u32 controlReg;
    XSpi_Config *spiConfig;

    // Initialize LCD DC GPIO
    status = XGpio_Initialize(&dc, XPAR_SPI_DC_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Initialize GPIO dc fail!\n\r");
        return;
    }
    XGpio_SetDataDirection(&dc, 1, 0x0);  // Set as output

    // Initialize encoder GPIO
    status = XGpio_Initialize(&encoder, XPAR_ENCODER_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Initialize encoder fail!\n\r");
        return;
    }
    XGpio_SetDataDirection(&encoder, 1, 0xF);  // Set as input

    // Initialize Timer
    status = XTmrCtr_Initialize(&axiTimer, XPAR_AXI_TIMER_0_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Initialize timer fail!\n\r");
        return;
    }

    // Initialize interrupt controller and connect handlers
    status = XIntc_Initialize(&intc, XPAR_INTC_0_DEVICE_ID);
    status = XGpio_Initialize(&btnGpio, XPAR_AXI_GPIO_BTN_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Initialize interrupt controller fail!\n\r");
        return;
    }
    XGpio_SetDataDirection(&btnGpio, 1, 0xF);  // Set as input

    // Connect encoder and timer interrupts
    status = XIntc_Connect(&intc,
        XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR,
        (XInterruptHandler)encoder_handler,
        &encoder);
    XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
				(XInterruptHandler) button_handler, &btnGpio);
    if (status != XST_SUCCESS) {
        xil_printf("Connect encoder interrupt fail!\n\r");
        return;
    }

    status = XIntc_Connect(&intc,
        XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
        (XInterruptHandler)TimerCounterHandler,
        (void *)&axiTimer);
    if (status != XST_SUCCESS) {
        xil_printf("Connect timer interrupt fail!\n\r");
        return;
    }

    // Enable interrupts
    XGpio_InterruptEnable(&encoder, 1);
    XGpio_InterruptGlobalEnable(&encoder);

    XTmrCtr_SetOptions(&axiTimer, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_SetResetValue(&axiTimer, 0, 0xFFFFFFFF - RESET_VALUE);
    XTmrCtr_Start(&axiTimer, 0);

    // Start interrupt controller
    status = XIntc_Start(&intc, XIN_REAL_MODE);
    if (status != XST_SUCCESS) {
        xil_printf("Start interrupt controller fail!\n\r");
        return;
    }

    XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR);
    XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
    XGpio_InterruptEnable(&btnGpio, 1);
	XGpio_InterruptGlobalEnable(&btnGpio);
    XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);

    // Initialize SPI
    spiConfig = XSpi_LookupConfig(XPAR_SPI_DEVICE_ID);
    if (spiConfig == NULL) {
        xil_printf("Can't find spi device!\n\r");
        return;
    }

    status = XSpi_CfgInitialize(&spi, spiConfig, spiConfig->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("Initialize spi fail!\n\r");
        return;
    }

    // Configure SPI
    XSpi_Reset(&spi);
    controlReg = XSpi_GetControlReg(&spi);
    XSpi_SetControlReg(&spi,
        (controlReg | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK) &
        (~XSP_CR_TRANS_INHIBIT_MASK));
    XSpi_SetSlaveSelectReg(&spi, ~0x01);

    // Initialize LCD
    initLCD();

    // Clear screen with black background
    setColor(0, 0, 0);
    fillRect(0, 0, 240, 320);
}


/*..........................................................................*/
void QF_onStartup(void) {                 /* entered with interrupts locked */

/* Enable interrupts */
	xil_printf("\n\rQF_onStartup\n"); // Comment out once you are in your complete program

	xil_printf("QF_onStartup: Enabling interrupts\n\r");

	// Register interrupt controller handler
	microblaze_register_handler((XInterruptHandler)XIntc_InterruptHandler,
							  (void *)&intc);

	// Enable MicroBlaze interrupts
	microblaze_enable_interrupts();

	xil_printf("QF_onStartup: Interrupts enabled\n\r");
}


void QF_onIdle(void) {
    QF_INT_UNLOCK();

    if (AO_Lab2A.currentMode == 0) { // Standard mode
        // First pass - detect octave with no decimation
        read_fsl_values(q, SAMPLES, 1);
        sample_f = SAMPLE_RATE;  // 100*1000*1000/2048.0

        // Clear imaginary buffer
        for(int l = 0; l < SAMPLES; l++) {
            w[l] = 0;
        }

        // Initial FFT for octave detection
        float initial_freq = fft(q, w, SAMPLES, FFT_LOG2_SAMPLES, sample_f);

        // Determine factor based on octave
        int factor = 1;
        int octave = 4;  // Default to middle octave

        if (initial_freq > 0) {
            octave = (int)(log2(initial_freq/440.0) + 4.0);

            if (octave < 3) {
                factor = 64;
            } else if (octave < 7) {
                factor = 8;
            } else if (octave < 8) {
                factor = 4;
            } else if (octave < 9) {
                factor = 2;
            } else {
                factor = 1;
            }
        }

        // Second pass with appropriate decimation
        read_fsl_values(q, SAMPLES, factor);
        sample_f = SAMPLE_RATE/factor;

        // Clear imaginary buffer again
        for(int l = 0; l < SAMPLES; l++) {
            w[l] = 0;
        }

        // Final FFT with decimation
        float frequency = fft(q, w, SAMPLES, FFT_LOG2_SAMPLES, sample_f);

        // Validate frequency
        if (frequency <= 0 || frequency > 5000) {
            return;
        }

        // Calculate cents offset using current base frequency
        float nearestNote = getNearestNoteFreq(frequency, AO_Lab2A.currentFreq);
        int cents = calculateCents(frequency, nearestNote);

        // Clamp cents to reasonable range for display
        if (cents > 50) cents = 50;
        if (cents < -50) cents = -50;

        // Update active object
        AO_Lab2A.detectedFreq = frequency;
        AO_Lab2A.centOffset = cents;

        // Post event to update display
        QActive_postISR((QActive *)&AO_Lab2A, NEW_FREQ_EVENT);
    }
}

/* Q_onAssert is called only when the program encounters an error*/
/*..........................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    (void)file;                                   /* name of the file that throws the error */
    (void)line;                                   /* line of the code that throws the error */
    QF_INT_LOCK();
//    printDebugLog();
    for (;;) {
    }
}

/* Interrupt handler functions here.  Do not forget to include them in lab2a.h!
To post an event from an ISR, use this template:
QActive_postISR((QActive *)&AO_Lab2A, SIGNALHERE);
Where the Signals are defined in lab2a.h  */

/******************************************************************************
*
* This is the interrupt handler routine for the GPIO for this example.
*
******************************************************************************/
void GpioHandler(void *CallbackRef) {
	// Increment A counter
}

void button_handler(void *CallbackRef) {
    XGpio *GpioPtr = (XGpio *)CallbackRef;
    unsigned int buttons = XGpio_DiscreteRead(&btnGpio, 1);

    lastActivityTime = 0;  // Reset activity timer for any button press

    if (buttons & 0x01) {
        QActive_postISR((QActive *)&AO_Lab2A, BTN1_PRESS);
    }      // Button 1
    else if (buttons & 0x02) {// Button 2
        QActive_postISR((QActive *)&AO_Lab2A, BTN2_PRESS);
    }
    else if (buttons & 0x04) {// Button 3
        xil_printf("BTN3 PRESS\n\r");
        QActive_postISR((QActive *)&AO_Lab2A, BTN3_PRESS);
    }
    else if (buttons & 0x08) {// Button 4
        xil_printf("BTN4 PRESS\n\r");
        QActive_postISR((QActive *)&AO_Lab2A, BTN4_PRESS);
    }

    XGpio_InterruptClear(GpioPtr, 1);
}

void encoder_handler(void *CallbackRef) {
    XGpio *GpioPtr = (XGpio *)CallbackRef;
    unsigned int encoder_state = XGpio_DiscreteRead(GpioPtr, 1);
    static unsigned int previous_state = 3;  // Default idle state

    if (encoder_state == 7 && previous_state != 7) {  // Button press detected
        button_press_pending = 1;
        last_button_press_time = count;
        lastActivityTime = 0;
        QActive_postISR((QActive *)&AO_Lab2A, ENCODER_CLICK);
    }
    else if (encoder_state != previous_state) {  // Only process when state changes
        // Extract A and B signals from encoder state
        int A = (encoder_state & 0x01);
        int B = (encoder_state & 0x02) >> 1;
        int input = (A << 1) | B;

        // State machine for direction detection
        switch (currentState) {
            case S0:  // Initial state
                if (input == 0b01) {
                    currentState = S1;  // Moving clockwise
                } else if (input == 0b10) {
                    currentState = S4;  // Moving counter-clockwise
                }
                break;

            case S1:  // First clockwise state
                if (input == 0b11) {
                    currentState = S2;
                } else if (input == 0b00) {
                    currentState = S0;
                }
                break;

            case S2:  // Second clockwise state
                if (input == 0b10) {
                    currentState = S3;
                } else if (input == 0b01) {
                    currentState = S1;
                }
                break;

            case S3:  // Final clockwise state
                if (input == 0b00) {
                    currentState = S0;
                    lastActivityTime = 0;
                    QActive_postISR((QActive *)&AO_Lab2A, ENCODER_UP);
                }
                break;

            case S4:  // First counter-clockwise state
                if (input == 0b11) {
                    currentState = S5;
                } else if (input == 0b00) {
                    currentState = S0;
                }
                break;

            case S5:  // Second counter-clockwise state
                if (input == 0b01) {
                    currentState = S6;
                } else if (input == 0b10) {
                    currentState = S4;
                }
                break;

            case S6:  // Final counter-clockwise state
                if (input == 0b00) {
                    currentState = S0;
                    lastActivityTime = 0;
                    QActive_postISR((QActive *)&AO_Lab2A, ENCODER_DOWN);
                }
                break;

            default:
                currentState = S0;
                break;
        }
    }

    previous_state = encoder_state;
    XGpio_InterruptClear(GpioPtr, 1);
}

void TwistHandler(void *CallbackRef) {
	//XGpio_DiscreteRead( &twist_Gpio, 1);

}

void debounceTwistInterrupt(){
	// Read both lines here? What is twist[0] and twist[1]?
	// How can you use reading from the two GPIO twist input pins to figure out which way the twist is going?
}

void debounceInterrupt() {
	QActive_postISR((QActive *)&AO_Lab2A, ENCODER_CLICK);
	// XGpio_InterruptClear(&sw_Gpio, GPIO_CHANNEL1); // (Example, need to fill in your own parameters
}

void read_fsl_values(float* q, int n, int decimate) {
   int i;
   unsigned int x;
   stream_grabber_start(); //mawga doesnt have this
   stream_grabber_wait_enough_samples(SAMPLES/decimate); //mawga doesnt have this

   for(i = 0; i < n*decimate; i+=decimate) {
      int_buffer[i/decimate] = stream_grabber_read_sample(i);
      // xil_printf("%d\n",int_buffer[i]);
      x = int_buffer[i/decimate];
      q[i/decimate] = 3.3*x/67108864.0; // 3.3V and 2^26 bit precision.

   }
}
