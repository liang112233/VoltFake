#include <fcntl.h>
#include <immintrin.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

int fd;
uint64_t plane_zero;

uint64_t wrmsr_value(int64_t val,uint64_t plane)
{
        // -0.5 to deal with rounding issues
        val=(val*1.024)-0.5;
        val=0xFFE00000&((val&0xFFF)<<21);
        val=val|0x8000001100000000;
        val=val|(plane<<40);
        return (uint64_t)val;
}

void voltage_change(int fd, uint64_t val)
{
        pwrite(fd,&val,sizeof(val),0x150);
}


int main()
{
fd = open("/dev/cpu/0/msr", O_WRONLY);
	if(fd == -1)
	{
		printf("Could not open /dev/cpu/0/msr\n");
		return -1;
	}

plane_zero = wrmsr_value( -200, 0);

voltage_change(fd,plane_zero);


}	


