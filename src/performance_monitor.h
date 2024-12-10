#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <stdint.h>

// Define monitored regions
#define NUM_REGIONS 20
typedef struct {
    uint32_t start_addr;
    uint32_t end_addr;
    uint32_t count;
    const char* name;
} CodeRegion;

// Function declarations
void init_performance_monitor(void);
void start_monitoring(void);
void stop_monitoring(void);
void print_performance_results(void);

#endif
