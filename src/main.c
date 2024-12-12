//
//#include <stdio.h>
//#include "xil_cache.h"
//#include <mb_interface.h>
//#include "xparameters.h"
//#include <xil_types.h>
//#include <xil_assert.h>
//#include <xio.h>
//#include "xtmrctr.h"
//#include "fft.h"
//#include "note.h"
//#include "stream_grabber.h"
//#include "performance_monitor.h"  // Add this include
//
//// State machine
//#include "qp/qpn_port.h"                                       /* QP-nano port */
//#include "qp/bsp.h"                             /* Board Support Package (BSP) */
//#include "qp/lab2a.h"                               /* application interface */
//
//static QEvent l_lab2aQueue[30];
//
//QActiveCB const Q_ROM Q_ROM_VAR QF_active[] = {
//	{ (QActive *)0,            (QEvent *)0,          0                    },
//	{ (QActive *)&AO_Lab2A,    l_lab2aQueue,         Q_DIM(l_lab2aQueue)  }
//};
//
//#define SAMPLES 512 // AXI4 Streaming Data FIFO has size 512
//#define M 9 //2^m=samples
//#define CLOCK 100000000.0 //clock speed
//
//int int_buffer[SAMPLES];
//static float q[SAMPLES];
//static float w[SAMPLES];
//
//void read_fsl_values(float* q, int n, int averaging_factor) {
//    int i, j;
//    unsigned int x;
//    int32_t sum;
//
//    stream_grabber_start();
//    stream_grabber_wait_enough_samples(256);
//
//    // Process averaging_factor samples at a time
//    for(i = 0; i < n; i++) {
//        sum = 0;
//        // Sum averaging_factor samples together
//        for(j = 0; j < averaging_factor; j++) {
//            sum += stream_grabber_read_sample(i * averaging_factor + j);
//        }
//        // Note: we don't divide by averaging_factor as mentioned in the doc
//        // Convert to voltage after averaging to reduce floating point operations
//        int32_t shifted = sum >> 26;
//        q[i] = 3.3 * shifted;
//    }
//}
//
//int main() {
//	// Initialize FFT lookup tables
//	init_fft_tables();
//
//	// Choose averaging factor based on frequency range
//	// For example: 4 for low frequencies, 1 for high frequencies
//	int averaging_factor = 4;  // Adjust based on your needs
//	int effective_samples = SAMPLES / averaging_factor;
//
//   float sample_f;
//   int l;
//   int ticks;
//   float frequency;
//   float tot_time;
//   int samples_processed = 0;  // Add counter for samples
//
//   uint32_t Control;
//
//   Xil_ICacheInvalidate();
//   Xil_ICacheEnable();
//   Xil_DCacheInvalidate();
//   Xil_DCacheEnable();
//
//   // Initialize state machine
//   BSP_init();         // Initialize hardware first
//   Lab2A_ctor();
////   QF_run();
//
//   // Set up main timer
//   XTmrCtr timer;
//   XTmrCtr_Initialize(&timer, XPAR_AXI_TIMER_0_DEVICE_ID);
//   Control = XTmrCtr_GetOptions(&timer, 0) | XTC_CAPTURE_MODE_OPTION | XTC_INT_MODE_OPTION;
//   XTmrCtr_SetOptions(&timer, 0, Control);
//
//   // Initialize and start performance monitor
//   init_performance_monitor();
//   start_monitoring();
//
//   print("Starting performance analysis...\n\r");
//
//   while(1) {
//	   XTmrCtr_Start(&timer, 0);
//
//	   read_fsl_values(q, effective_samples, averaging_factor);
//	   sample_f = (100*1000*1000/2048.0) / averaging_factor;  // Adjust sample rate
//
//	   // Zero w array
//	   memset(w, 0, effective_samples * sizeof(float));
//
//	   frequency = fft(q, w, effective_samples, log2(effective_samples), sample_f);
//      findNote(frequency);
//
//      ticks = XTmrCtr_GetValue(&timer, 0);
//      XTmrCtr_Stop(&timer, 0);
//      tot_time = ticks/CLOCK;
//      xil_printf("frequency: %d Hz\r\n",(int)(frequency+.5));
//
//      samples_processed++;
//      if(samples_processed >= 500000000) {  // After 100 samples
//          stop_monitoring();
//          print_performance_results();
//          break;  // Exit the while loop
//      }
//   }
//
//   return 0;
//}

#include "qp/qpn_port.h"
#include "qp/bsp.h"
#include "qp/lab2a.h"
#include "xil_cache.h"
#include "fft.h"
#include "stream_grabber.h"
#include "performance_monitor.h"

static QEvent l_lab2aQueue[30];

QActiveCB const Q_ROM Q_ROM_VAR QF_active[] = {
    { (QActive *)0,            (QEvent *)0,          0                    },
    { (QActive *)&AO_Lab2A,    l_lab2aQueue,         Q_DIM(l_lab2aQueue)  }
};

Q_ASSERT_COMPILE(QF_MAX_ACTIVE == Q_DIM(QF_active) - 1);

int main(void) {
    // Cache setup
    Xil_ICacheInvalidate();
    Xil_ICacheEnable();
    Xil_DCacheInvalidate();
    Xil_DCacheEnable();

    // Initialize FFT and peripherals
    LUT_construct();
//    init_performance_monitor();
//    start_monitoring();
    stream_grabber_start();

    // Initialize QP-nano system
    BSP_init();         // Initialize hardware first
    Lab2A_ctor();       // Construct the active object

    // Start the QP-nano framework
    QF_run();          // This function never returns

    return 0;
}
