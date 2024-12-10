#include "performance_monitor.h"
#include "xparameters.h"
#include "xtmrctr.h"
#include "xintc.h"
#include <stdlib.h>
#include <stdio.h>

static XTmrCtr axiTimer;
static XIntc intc;

static CodeRegion regions[NUM_REGIONS] = {
    // Core functions
    {0x800033fc, 0x800033fc + 0x17c, 0, "read_fsl_values"},
    {0x80002ae4, 0x80002ae4 + 0x918, 0, "fft"},
    {0x80003938, 0x80003938 + 0x1f8, 0, "findNote"},
    {0x80003578, 0x80003578 + 0x3c0, 0, "main"},

    // Stream and Timer operations
    {0x80004150, 0x80004150 + 0x30, 0, "stream_grabber_start"},
    {0x80004180, 0x80004180 + 0x48, 0, "stream_grabber_wait"},
    {0x80003b9c, 0x80003b9c + 0x134, 0, "TimerHandler"},
    {0x800057b0, 0x800057b0 + 0x5c, 0, "InterruptHandler"},

    // Math functions
    {0x80001ee0, 0x80001ee0 + 0xfc, 0, "log"},
    {0x80004264, 0x80004264 + 0x1d0, 0, "sine"},
    {0x80004434, 0x80004434 + 0x1fc, 0, "cosine"},

    // Memory operations
    {0x80001aa0, 0x80001aa0 + 0x128, 0, "memcpy"},
    {0x80001bc8, 0x80001bc8 + 0x114, 0, "memset"},

    // Output/Print functions
    {0x80004ba0, 0x80004ba0 + 0x464, 0, "xil_vprintf"},
    {0x80005004, 0x80005004 + 0x38, 0, "xil_printf"},
    {0x8000499c, 0x8000499c + 0x204, 0, "outnum"},
    {0x8000503c, 0x8000503c + 0x2c, 0, "outbyte"},

    // Floating point operations
    {0x800007a8, 0x800007a8 + 0x70, 0, "__adddf3"},
    {0x80000894, 0x80000894 + 0x39c, 0, "__muldf3"},

    // Everything else
    {0x80000000, 0x80006000, 0, "other"}
};

static void TimerHandler(void *CallBackRef, uint8_t TmrCtrNumber) {
    uint32_t ControlStatusReg;
    uint32_t pc;

    // Clear the timer interrupt first
    ControlStatusReg = XTmrCtr_ReadReg(axiTimer.BaseAddress, 0, XTC_TCSR_OFFSET);
    XTmrCtr_WriteReg(axiTimer.BaseAddress, 0, XTC_TCSR_OFFSET,
                     ControlStatusReg | XTC_CSR_INT_OCCURED_MASK);

//    // Get PC
    asm("add %0, r0, r14" : "=r"(pc));
//    xil_printf("Timer interrupt, PC = 0x%x\r\n", pc);

    // Update counters
    for(int i = 0; i < NUM_REGIONS; i++) {
        if(pc >= regions[i].start_addr && pc < regions[i].end_addr) {
            regions[i].count++;
            break;
        }
    }

    // No need to stop/reset/start timer since we're using auto-reload
}

void init_performance_monitor(void) {
    XStatus status;

    // Initialize Timer
    status = XTmrCtr_Initialize(&axiTimer, XPAR_AXI_TIMER_1_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Timer initialization failed!\r\n");
        return;
    }

    // Initialize interrupt controller
    status = XIntc_Initialize(&intc, XPAR_INTC_0_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Interrupt controller initialization failed!\r\n");
        return;
    }

    // Connect Timer Interrupt
    status = XIntc_Connect(&intc,
                          XPAR_INTC_0_TMRCTR_1_VEC_ID,  // Make sure this matches your hardware
                          (XInterruptHandler)TimerHandler,
                          (void *)&axiTimer);
    if (status != XST_SUCCESS) {
        xil_printf("Timer interrupt connection failed!\r\n");
        return;
    }

    // Set up timer options
    uint32_t options = XTC_INT_MODE_OPTION |
                      XTC_AUTO_RELOAD_OPTION |
                      XTC_DOWN_COUNT_OPTION;

    XTmrCtr_SetOptions(&axiTimer, 0, options);

    // Set timer period (1ms)
    uint32_t timerPeriod = XPAR_CPU_CORE_CLOCK_FREQ_HZ / 1000;
    XTmrCtr_SetResetValue(&axiTimer, 0, timerPeriod);

    // Start interrupt controller
    status = XIntc_Start(&intc, XIN_REAL_MODE);
    if (status != XST_SUCCESS) {
        xil_printf("Interrupt controller start failed!\r\n");
        return;
    }

    // Enable the timer interrupt in the interrupt controller
    XIntc_Enable(&intc, XPAR_INTC_0_TMRCTR_1_VEC_ID);

    // Register the interrupt controller handler with the Microblaze
    microblaze_register_handler((XInterruptHandler)XIntc_InterruptHandler, &intc);

    // Enable interrupts in the Microblaze
    microblaze_enable_interrupts();

    xil_printf("Performance monitor initialized\r\n");
}

void start_monitoring(void) {
    // Clear all counts
    for(int i = 0; i < NUM_REGIONS; i++) {
        regions[i].count = 0;
    }

    // Load the timer counter
    XTmrCtr_SetResetValue(&axiTimer, 0, XPAR_CPU_CORE_CLOCK_FREQ_HZ / 100000);

    // Start the timer
    XTmrCtr_Start(&axiTimer, 0);

    xil_printf("Monitoring started\r\n");
}

void stop_monitoring(void) {
    XTmrCtr_Stop(&axiTimer, 0);
    xil_printf("Monitoring stopped\r\n");
}

void print_performance_results(void) {
    uint32_t total_samples = 0;

    xil_printf("\r\nPerformance Analysis Results:\r\n");
    for(int i = 0; i < NUM_REGIONS; i++) {
        total_samples += regions[i].count;
    }

    if(total_samples == 0) {
        xil_printf("No samples collected!\r\n");
        return;
    }
    xil_printf("Total samples: %d\r\n", total_samples);

    for(int i = 0; i < NUM_REGIONS; i++) {

    	uint32_t percentage = (regions[i].count * 100) / total_samples;
    	xil_printf("percentage: %d\r\n", percentage);
    	xil_printf("%s: %lu samples (%d%%)\r\n",
                   regions[i].name, regions[i].count, percentage);
    }
}
