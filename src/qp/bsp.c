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

int calculateCents(float freq, float baseFreq) {
    // Use the formula: cents = 1200 * log2(freq/baseFreq)
    return (int)(1200.0 * log2(freq/baseFreq));
}

// Get the nearest note frequency
float getNearestNoteFreq(float freq) {
    float c4 = 261.63; // C4 as reference
    float semitones = 12.0 * log2(freq/c4);
    int nearest = (int)(semitones + 0.5);
    return c4 * pow(2.0, nearest/12.0);
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
    if (status != XST_SUCCESS) {
        xil_printf("Initialize interrupt controller fail!\n\r");
        return;
    }

    // Connect encoder and timer interrupts
    status = XIntc_Connect(&intc,
        XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR,
        (XInterruptHandler)encoder_handler,
        &encoder);
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

    static const int averaging_factor = 4;
    static const float sample_f = SAMPLE_RATE / averaging_factor;

    // Read new audio samples
    read_fsl_values(q, SAMPLES/averaging_factor, averaging_factor);

    // Zero the imaginary buffer
    memset(w, 0, (SAMPLES/averaging_factor) * sizeof(float));

    // Run FFT
    float frequency = fft(q, w, SAMPLES/averaging_factor,
                         FFT_LOG2_SAMPLES - 2,
                         sample_f);

    // Calculate cents offset using the current base frequency
    float nearestNote = getNearestNoteFreq(frequency);
    int cents = calculateCents(frequency, nearestNote);

    // Update active object
    AO_Lab2A.detectedFreq = frequency;
    AO_Lab2A.centOffset = (int)cents;

    // Post event to update display
    QActive_postISR((QActive *)&AO_Lab2A, NEW_FREQ_EVENT);
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
    unsigned int buttons = XGpio_DiscreteRead(GpioPtr, 1);

    lastActivityTime = 0;  // Reset activity timer for any button press

    if (buttons & 0x01)      // Button 1
        QActive_postISR((QActive *)&AO_Lab2A, BTN1_PRESS);
    else if (buttons & 0x02) // Button 2
        QActive_postISR((QActive *)&AO_Lab2A, BTN2_PRESS);
    else if (buttons & 0x04) // Button 3
        QActive_postISR((QActive *)&AO_Lab2A, BTN3_PRESS);
    else if (buttons & 0x08) // Button 4
        QActive_postISR((QActive *)&AO_Lab2A, BTN4_PRESS);

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

void read_fsl_values(float* q, int n, int averaging_factor) {
    int i, j;
    unsigned int x;
    int32_t sum;

    stream_grabber_start();
    stream_grabber_wait_enough_samples(1024);

    // Process averaging_factor samples at a time
    for(i = 0; i < n; i++) {
        sum = 0;
        // Sum averaging_factor samples together
        for(j = 0; j < averaging_factor; j++) {
            sum += stream_grabber_read_sample(i * averaging_factor + j);
        }
        // Note: we don't divide by averaging_factor as mentioned in the doc
        // Convert to voltage after averaging to reduce floating point operations
        int32_t shifted = sum >> 26;
        q[i] = 3.3 * shifted;
    }
}
