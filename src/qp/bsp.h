/*****************************************************************************
* bsp.h for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
*****************************************************************************/
#ifndef bsp_h
#define bsp_h

#include <math.h>
#include "../globals.h"

#define RESET_VALUE 1000
#define SAMPLE_RATE (100.0*1000*1000/2048.0)

void BSP_init(void);
void ISR_gpio(void);
void ISR_timer(void);
void checkInterruptStatus(void);

void read_fsl_values(float* q, int n, int averaging_factor);
int findNoteAndCents(float f, float baseA4, int* cents);

//void printDebugLog(void);

#define BSP_showState(prio_, state_) ((void)0)


#endif


