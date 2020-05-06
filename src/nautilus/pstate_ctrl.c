#include <nautilus/nautilus.h>


// AMD 

#define AMD_MSR_pstate_cur_lim_addr 0xC0010061

typedef union  
{
    struct 
    {
        uint64_t reserved : 56;
        uint8_t pstate_max_val : 4;
        uint8_t pstate_cur_lim : 4;
    };
    uint64_t value;

} __packed AMD_MSR_pstate_cur_lim_t;