#include <nautilus/nautilus.h>
#include "dvsf.h"


static int is_intel (void);
static int get_cpu_vendor (char name[13]);

// Core information
struct pstate_data {
          uint64_t     current_pstate;
          uint64_t      min_pstate;
          uint64_t      max_pstate;				// Curret - sw?
          uint64_t      max_pstate_physical;  /// HW imposed max?

		  // Added freq info in KHz
		  uint64_t		current_freq_khz;
		  uint64_t		min_freq_khz;
		  uint64_t		max_freq_khz;

		// ?
          uint64_t      scaling;

		  // turbo boost related
          uint64_t     turbo_pstate;
          unsigned int max_freq;
          unsigned int turbo_freq;

		// Copied from palacios . turbo stuff
		uint8_t prior_speedstep;
		uint8_t turbo_disabled;
		uint8_t no_turbo;

		// Copied from linux cpu freq policy struct // line 68, cpufreq.h
		unsigned int		restore_freq; /* = policy->cur before transition */
		unsigned int		suspend_freq;
		struct cpufreq_frequency_table	*freq_table; // already filled (hard!!!) prediction? (like bp)
		unsigned int		transition_delay_us; // ? nice to have. Maybe do it. CPUID?? 
  };

struct vid_data {
          int min;
          int max;
          int turbo;
          int32_t ratio;
};


struct cpu {

	cpu_id_t id;
	enum {AMD, INTEL, OTHER} arch;
	struct pstate_data pstate;
		
	// what functionality it has 

	// max freq
	
	// min freq
	
	// cur_freq



};

struct cpu my_cpu;
#define IA32_PM_ENABLE_MSR 0x00000770 
struct ia32_pm_enable_msr {
	union {
		uint64_t val;
		struct {
			uint64_t rsvd: 63;
			uint8_t  hwp_enable: 1;
		}reg;
	}__attribute__(__packed__);
}__attribute__(__packed__);

#define IA32_PERF_CTL 0x00000199

struct ia32_perf_ctl {
	union {
		uint64_t val;
		struct {
			uint16_t pstate : 16;
			uint16_t rsvrd  : 16;
			uint8_t  turbo  : 1;
			uint32_t rvsd2  : 31;	
		}reg;

	}__attribute__(__packed__);
}__attribute__(__packed__);

#define MSR_MPERF_IA32         0x000000e7
#define MSR_APERF_IA32         0x000000e8


int set_pstate(int pstate) {
	
	struct ia32_perf_ctl val;	

	if (my_cpu.pstate.current_pstate == pstate)
		return 0;
	if(pstate < my_cpu.min_pstate) 
		return 1;
	if(pstate > my_cpu.max_pstate) 
		return 1;

	val = read_msr(IA32_PERF_CTL);
	
	
}

int dvfs_init(void) {
	/* declarations */
	cpuid_ret_t regs;
	int supports_speedstep, supports_hw_cord, supports_hwp; 
	struct ia32_pm_enable_msr val;
	
	/* get cpu characteristics */
	if(is_intel()) 
		my_cpu.arch = INTEL;
		
	else
		return 1;

	/* check for intel speedstep */
	if(cpuid(0x1, &regs) != 0)
		return 1;	
	supports_speedstep = !!(regs.c & (1 << 7));

	/* check for hardware cordination */	
	if(cpuid(0x6, &regs) != 0)
		return 1; 

	supports_hw_cord = !!(regs.c & (1 << 15));
	supports_hwp = !!(regs.a & (1 << 7));
	supports_hwp_not = !!(regs.a & (1 << 8)); 
	supports_act_wind_cont = !!(regs.a & (1 << 9));  
	supports_pref_cont = !!(regs.a & (1 << 10));  
	supports_pack_cont = !!(regs.a & (1 << 11));  
	/* disable hwp */	
	if(supports_hwp) {
		val = msr_read(IA32_PM_ENABLE_MSR);
		val.reg.hwp_enable = 0;
		msr_write(IA32_PM_ENABLE_MSR, val);
	}
	/*disable turbo */	
	set_turbo(0);	

}

static inline void set_turbo(int enable) 
{
	struct ia32_perf_ctl val;
	uint8_t en_bit;
	en_bit = !!(enable);
	val = read_msr(IA32_PERF_CTL);
	val = val.reg.turbo = en_bit;
	msr_write(IA32_PERF_CTL, val);
		
} 

static inline void cpuid_string (uint32_t id, uint32_t dest[4]) 
{
    asm volatile("cpuid"
            :"=a"(*dest),"=b"(*(dest+1)),"=c"(*(dest+2)),"=d"(*(dest+3))
            :"a"(id));
}

static int is_intel (void)
{
    char name[13];
    get_cpu_vendor(name);
    return !strcmp(name,"GenuineIntel");
}


static int get_cpu_vendor (char name[13])
{
    uint32_t dest[4];
    uint32_t maxid;

    cpuid_string(0,dest);
    maxid=dest[0];
    ((uint32_t*)name)[0]=dest[1];
    ((uint32_t*)name)[1]=dest[3];
    ((uint32_t*)name)[2]=dest[2];
    name[12]=0;

    return maxid;
}
