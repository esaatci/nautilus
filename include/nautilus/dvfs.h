// Header Guard

#include <nautilus/nautilus.h>
//#include msr.h
// #include cpuid.h

// #defines

// static function in dvfs.c
// get pstate()
// set pstate()
// aperfmperf snapshot()
// related to turbo boost


// Get available frequency levels. We return the freq table.
uint64_t* get_available_freq(void);

// Get freq of current CPU. Read MSR MPERF/APERF, calculate
// CPUID[edx] => arch max frequency.    <=== Called in init or someplace earlier.
uint64_t get_freq(void);

// Set freq of current CPU. Write MSR (freq table mapping) Find the corresponding P state. 
// Then write that P state val to the correct MSR
void set_freq(uint64_t freq);

// Set freq of another CPU. Not sure how? Interrupt-related? percpu variables... yuck
void set_freq_others(uint64_t freq, uint8_t cpu);

// ADDED: HW dependencies ... CPUID to get coupled relationships
// Possibly return 2d matrix
uint64_t** get_topology(void);

// Init/deinit routines. Return status.
uint64_t dvfs_init(void);
uint64_t dvfs_deinit(void);