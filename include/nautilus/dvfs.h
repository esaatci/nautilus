
#ifndef __NK_DVFS__
#define __NK_DVFS__

#include <nautilus/nautilus.h>




// 1 for bogomips 0 for array
#define TABLE_METHOD 0

// Get available frequency levels. We return the freq table.
uint64_t* nk_get_available_freq(void);

// Get freq of current CPU. Read MSR MPERF/APERF, calculate
// CPUID[edx] => arch max frequency.    <=== Called in init or someplace earlier.
uint64_t nk_get_freq(void);

// Set freq of current CPU. Write MSR (freq table mapping) Find the corresponding P state. 
// Then write that P state val to the correct MSR
void nk_set_freq(uint64_t freq);

// Set freq of another CPU. Not sure how? Interrupt-related? percpu variables... yuck
void nk_set_freq_others(uint64_t freq, uint8_t cpu);

// Init/deinit routines. Return status.
uint64_t nk_dvfs_init(void);
uint64_t nk_dvfs_deinit(void);

#endif
