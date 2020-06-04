#include <nautilus/nautilus.h>
#include <nautilus/msr.h>
#include <nautilus/shell.h>
#include <nautilus/dvfs.h>
#include <nautilus/cpuid.h>
#include <nautilus/smp.h>
#include <nautilus/irq.h>

#define MSR_MPERF_IA32         	0x000000e7
#define MSR_APERF_IA32         	0x000000e8

#define IA32_PM_ENABLE_MSR 		0x00000770 
#define MSR_PERF_STAT_IA32     	0x00000198
#define IA32_PERF_CTL 			0x00000199
#define MSR_MISC_ENABLE_IA32  	0x000001a0

#define INFO(fmt, args...) INFO_PRINT("DVFS: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("DVFS: " fmt, ##args)
#define NAUT_CONFIG_DEBUG_DVFS 1

#define DEBUG(fmt, args...)
#ifdef NAUT_CONFIG_DEBUG_DVFS
#undef DEBUG
#define DEBUG(fmt, args...) DEBUG_PRINT("DVFS: " fmt, ##args)
#endif

static int is_intel (void);
static int get_cpu_vendor (char name[13]);
static int set_pstate(uint16_t pstate); 
static uint16_t get_pstate_intel(void);

// Core information
static struct pstate_data {
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
} pstate_data;

struct aperfmperf_sample {
	uint64_t	khz;
	uint64_t	time;
	uint64_t	aperf;
	uint64_t	mperf;
};

struct vid_data {
          int min;
          int max;
          int turbo;
          uint32_t ratio;
};

/*
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
*/

struct ia32_pm_enable_msr {
	union {
		uint64_t val;
		struct {
			uint64_t rsvd: 63;
			uint8_t  hwp_enable: 1;
		}reg;
	}__attribute__((packed));
}__attribute__((packed));

struct ia32_perf_ctl {
	union {
		uint64_t val;
		struct {
			uint16_t pstate : 16;
			uint16_t rsvrd  : 16;
			uint8_t  turbo  : 1;
			uint32_t rvsd2  : 31;	
		}reg;

	}__attribute__((packed));
}__attribute__((packed));

struct perf_stat_reg_intel {
    union {
        uint64_t val;
        struct {
            // this is the current
            uint16_t pstate                 : 16;
            uint64_t reserved               : 48;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));

static inline void set_turbo(int enable) 
{
	struct ia32_perf_ctl perf_ctl;
	uint8_t en_bit;
	en_bit = !!(enable);
	perf_ctl.val = msr_read(IA32_PERF_CTL);
	perf_ctl.reg.turbo = en_bit;
	msr_write(IA32_PERF_CTL, perf_ctl.val);
}

static int set_pstate(uint16_t pstate) {
	DEBUG("I call set_pstate().\n");
	nk_vc_printf("I call set_pstate().\n");
	struct ia32_perf_ctl perf_ctl;	
	/*
	if (my_cpu.pstate.current_pstate == pstate)
		return 0;
	if(pstate < my_cpu.min_pstate) 
		return 1;
	if(pstate > my_cpu.max_pstate) 
		return 1;
	*/
	perf_ctl.val = msr_read(IA32_PERF_CTL);
	nk_vc_printf("I read %016x\n", perf_ctl.val);
	perf_ctl.reg.pstate = pstate;
	pstate_data.current_pstate = pstate;
	nk_vc_printf("I write %016x\n", perf_ctl.val);
	msr_write(IA32_PERF_CTL, perf_ctl.val);

	// MAKE SURE MSR has been written to.
	nk_vc_printf("Done! As a result, we've written %016x\n", msr_read(IA32_PERF_CTL));
	return 0;
}

// Get Pstate
static uint16_t get_pstate_intel(void)
{
	struct perf_stat_reg_intel perf;
    perf.val = msr_read(MSR_PERF_STAT_IA32);
	nk_vc_printf("P-State: Get: %08x\n", perf.val);

    // should check if turbo is active, in which case 
    // this value is not the whole story
	// Maybe deal with it at the end

    return perf.reg.pstate;
}

// Get current cpu frequency
/*
smp_xcall (cpu_id_t cpu_id, 
           nk_xcall_func_t fun,
           void * arg,
           uint8_t wait)
*/
/* Flags
uint8_t flags = irq_disable_save();
irq_enable_restore(flags);
*/
static void aperfmperf_snapshot_khz(void *dummy)
{
	uint64_t aperf, aperf_delta;
	uint64_t mperf, mperf_delta;
	//struct aperfmperf_sample *s = this_cpu_ptr(&samples);
	nk_vc_printf("I'm getting the per cpu sample\n");
	struct aperfmperf_sample *s = per_cpu_get(sample);
	//unsigned long flags;
	nk_vc_printf("disabling interrupts\n");

	uint8_t flags = irq_disable_save();
	aperf = msr_read(MSR_MPERF_IA32);
	mperf = msr_read(MSR_APERF_IA32);
	irq_enable_restore(flags);

	nk_vc_printf("enabled interrupts\n");
	aperf_delta = aperf - s->aperf;
	mperf_delta = mperf - s->mperf;
	nk_vc_printf("aperf has: %016x\n", aperf);
	nk_vc_printf("mperf has: %016x\n", mperf);

	/*
	 * There is no architectural guarantee that MPERF
	 * increments faster than we can read it.
	 */
	if (mperf_delta == 0)
	{
		return;
	}

	nk_vc_printf("getting cpu_khz\n");
	ulong_t cur_khz = per_cpu_get(cpu_khz);

	nk_vc_printf("cpu_khz is: %016x\n", cur_khz);
	s->time = 0; // Need nautilus version
	s->aperf = aperf;
	s->mperf = mperf;
	s->khz = (cur_khz * aperf_delta) / mperf_delta;
	nk_vc_printf("the runtime khz is: %u\n", s->khz);

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

uint64_t dvfs_init(void) {
	// declarations 
	cpuid_ret_t regs;
	int supports_speedstep, supports_hw_cord, supports_hwp ,supports_act_wind_cont, supports_hwp_not; 
	int  supports_pref_cont,supports_pack_cont;  
	int supports_acpi;
	struct ia32_pm_enable_msr val;
	
	//get cpu characteristics 
	if(!is_intel()) 
		{ nk_vc_printf("NOT intel\n");
		return 1;}

	/*
	// check for intel speedstep 
	if(cpuid(0x1, &regs) != 0) {
		nk_vc_printf("CPUID failed\n");
		return 1;
	}	
	supports_speedstep = !!(regs.c & (1 << 7));
	nk_vc_printf("supports p_state: %d and the regs.c : %08x \n", supports_speedstep, regs.c);
	if(!supports_speedstep) {
	return -1;
	}	
	*/

	// check for intel acpi
	if(cpuid(0x1, &regs) != 0) {
		nk_vc_printf("CPUID failed\n");
		return 1;
	}	
	supports_acpi = !!(regs.d & (1 << 22));
	nk_vc_printf("supports p_state: %d and the regs.c : %08x \n", supports_acpi, regs.d);
	if(!supports_acpi) {
		return -1;
	}	

	/*
	// check for hardware cordination 	
	if(cpuid(0x6, &regs) != 0)
		return 1; 

	supports_hw_cord = !!(regs.c & (1 << 15));
	supports_hwp = !!(regs.a & (1 << 7));
	supports_hwp_not = !!(regs.a & (1 << 8)); 
	supports_act_wind_cont = !!(regs.a & (1 << 9));  
	supports_pref_cont = !!(regs.a & (1 << 10));  
	supports_pack_cont = !!(regs.a & (1 << 11));  
	// disable hwp 	
	if(supports_hwp) {
		val.val = msr_read(IA32_PM_ENABLE_MSR);
		val.reg.hwp_enable = 0;
		msr_write(IA32_PM_ENABLE_MSR, val.val);
	}
	//disable turbo 	
	*/
//	set_turbo(0);	

	// enable speedstep
	uint64_t temp;

	temp =   msr_read(MSR_MISC_ENABLE_IA32);
	nk_vc_printf("temp is %08x\n", temp);
        // enable speedstep (probably already on)
	temp |= 1 << 16;
	nk_vc_printf("new temp is %08x\n", temp);
    	msr_write(MSR_MISC_ENABLE_IA32, temp);	
	return supports_acpi;

}

static int handle_get_pstate(char * buf, void * priv)
{
	uint64_t val = get_pstate_intel();
	nk_vc_printf("0x%016x\n",val);
	return 0;
}

static int handle_set_pstate(char *buf, void *priv) {
	uint32_t state;	
	if(sscanf(buf, "set_pstate %d", &state) == 1) {
	
		if(set_pstate(state)) {
			nk_vc_printf("can't set pstate\n");
		}
	}
	else {
	nk_vc_printf("can't parse the command\n");
	}
	
	return 0;
}

static int handle_dvfs_init(char * buf, void * priv)
{
	int res = dvfs_init();
	//set_turbo(1);
	nk_vc_printf("res is: %d\n",res);
	return 0;
}
// Shell command for snapshot aperfmperf
static int handle_aperfmpref_snapshot(char * buf, void * priv)
{
	nk_vc_printf("calling snapshot \n");
	aperfmperf_snapshot_khz(NULL);
	return 0;
}
static struct shell_cmd_impl handle_snapshot_impl = {
    .cmd      = "snapshot",
    .help_str = "Gets the snapshot of current kHz!",
    .handler  = handle_aperfmpref_snapshot
};

static struct shell_cmd_impl get_pstate_impl = {
    .cmd      = "get_pstate",
    .help_str = "Gets the pstate!",
    .handler  = handle_get_pstate
};

static struct shell_cmd_impl set_pstate_impl = {
    .cmd      = "set_pstate",
    .help_str = "Sets the pstate!",
    .handler  = handle_set_pstate
};

static struct shell_cmd_impl handle_dvfs_impl = {
    .cmd      = "dvfs",
    .help_str = "inits dvfs",
    .handler  = handle_dvfs_init,
};



nk_register_shell_cmd(handle_snapshot_impl);
nk_register_shell_cmd(get_pstate_impl);
nk_register_shell_cmd(set_pstate_impl);
nk_register_shell_cmd(handle_dvfs_impl);
