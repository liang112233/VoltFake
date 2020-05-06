
/*
10MHzClock.c
2014-10-24
Public Domain

gcc -o 10MHzClock 10MHzClock.c
sudo ./10MHzClock

*/

/*

This allows the setting of the three general purpose clocks.

The clocks are named GPCLK0, GPCLK1, and GPCLK2.

The clocks are accessible from the following gpios.

gpio4  GPCLK0 ALT0
gpio5  GPCLK1 ALT0 B+ and compute module only (reserved for system use)
gpio6  GPCLK2 ALT0 B+ and compute module only
gpio20 GPCLK0 ALT5 B+ and compute module only
gpio21 GPCLK1 ALT5 Not available on Rev.2 B (reserved for system use)

gpio32 GPCLK0 ALT0 Compute module only
gpio34 GPCLK0 ALT0 Compute module only
gpio42 GPCLK1 ALT0 Compute module only (reserved for system use)
gpio43 GPCLK2 ALT0 Compute module only
gpio44 GPCLK1 ALT0 Compute module only (reserved for system use)

Clock sources

0     0 Hz     Ground
1     19.2 MHz oscillator 
2     0 Hz     testdebug0
3     0 Hz     testdebug1
4     0 Hz     PLLA
5     1000 MHz PLLC (changes with overclock settings)
6     500 MHz  PLLD
7     216 MHz  HDMI auxiliary
8-15  0 Hz     Ground

The integer divider may be 2-4095.
The fractional divider may be 0-4095.

There is no 25MHz cap for using non-zero MASH
(multi-stage noise shaping) values.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SYST_BASE  0x20003000
#define DMA_BASE   0x20007000
#define CLK_BASE   0x20101000
#define GPIO_BASE  0x20200000
#define UART0_BASE 0x20201000
#define PCM_BASE   0x20203000
#define SPI0_BASE  0x20204000
#define I2C0_BASE  0x20205000
#define PWM_BASE   0x2020C000
#define UART1_BASE 0x20215000
#define I2C1_BASE  0x20804000
#define I2C2_BASE  0x20805000
#define DMA15_BASE 0x20E05000

#define DMA_LEN   0x1000 /* allow access to all channels */
#define CLK_LEN   0xA8
#define GPIO_LEN  0xB4
#define SYST_LEN  0x1C
#define PCM_LEN   0x24
#define PWM_LEN   0x28
#define I2C_LEN   0x1C

#define GPSET0 7
#define GPSET1 8

#define GPCLR0 10
#define GPCLR1 11

#define GPLEV0 13
#define GPLEV1 14

#define GPPUD     37
#define GPPUDCLK0 38
#define GPPUDCLK1 39

#define SYST_CS  0
#define SYST_CLO 1
#define SYST_CHI 2

#define CLK_PASSWD  (0x5A<<24)

#define CLK_CTL_MASH(x)((x)<<9)
#define CLK_CTL_BUSY    (1 <<7)
#define CLK_CTL_KILL    (1 <<5)
#define CLK_CTL_ENAB    (1 <<4)
#define CLK_CTL_SRC(x) ((x)<<0)

#define CLK_SRCS 4

#define CLK_CTL_SRC_OSC  1  /* 19.2 MHz */
#define CLK_CTL_SRC_PLLC 5  /* 1000 MHz */
#define CLK_CTL_SRC_PLLD 6  /*  500 MHz */
#define CLK_CTL_SRC_HDMI 7  /*  216 MHz */

#define CLK_DIV_DIVI(x) ((x)<<12)
#define CLK_DIV_DIVF(x) ((x)<< 0)

#define CLK_GP0_CTL 28
#define CLK_GP0_DIV 29
#define CLK_GP1_CTL 30
#define CLK_GP1_DIV 31
#define CLK_GP2_CTL 32
#define CLK_GP2_DIV 33

#define CLK_PCM_CTL 38
#define CLK_PCM_DIV 39

#define CLK_PWM_CTL 40
#define CLK_PWM_DIV 41


static volatile uint32_t  *gpioReg = MAP_FAILED;
static volatile uint32_t  *clkReg  = MAP_FAILED;

#define PI_BANK (gpio>>5)
#define PI_BIT  (1<<(gpio&0x1F))

/* gpio modes. */

#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

void gpioSetMode(unsigned gpio, unsigned mode)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);
}

int gpioGetMode(unsigned gpio)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   return (*(gpioReg + reg) >> shift) & 7;
}

static int initClock(int clock, int source, int divI, int divF, int MASH)
{
   int ctl[] = {CLK_GP0_CTL, CLK_GP2_CTL};
   int div[] = {CLK_GP0_DIV, CLK_GP2_DIV};
   int src[CLK_SRCS] =
      {CLK_CTL_SRC_PLLD,
       CLK_CTL_SRC_OSC,
       CLK_CTL_SRC_HDMI,
       CLK_CTL_SRC_PLLC};

   int clkCtl, clkDiv, clkSrc;
   uint32_t setting;

   if ((clock  < 0) || (clock  > 1))    return -1;
   if ((source < 0) || (source > 3 ))   return -2;
   if ((divI   < 2) || (divI   > 4095)) return -3;
   if ((divF   < 0) || (divF   > 4095)) return -4;
   if ((MASH   < 0) || (MASH   > 3))    return -5;

   clkCtl = ctl[clock];
   clkDiv = div[clock];
   clkSrc = src[source];

   clkReg[clkCtl] = CLK_PASSWD | CLK_CTL_KILL;

   /* wait for clock to stop */

   while (clkReg[clkCtl] & CLK_CTL_BUSY)
   {
      usleep(10);
   }

   clkReg[clkDiv] =
      (CLK_PASSWD | CLK_DIV_DIVI(divI) | CLK_DIV_DIVF(divF));

   usleep(10);

   clkReg[clkCtl] =
      (CLK_PASSWD | CLK_CTL_MASH(MASH) | CLK_CTL_SRC(clkSrc));

   usleep(10);

   clkReg[clkCtl] |= (CLK_PASSWD | CLK_CTL_ENAB);
}

static int termClock(int clock)
{
   int ctl[] = {CLK_GP0_CTL, CLK_GP2_CTL};

   int clkCtl;

   if ((clock  < 0) || (clock  > 1))    return -1;

   clkCtl = ctl[clock];

   clkReg[clkCtl] = CLK_PASSWD | CLK_CTL_KILL;

   /* wait for clock to stop */

   while (clkReg[clkCtl] & CLK_CTL_BUSY)
   {
      usleep(10);
   }
}


/* Map in registers. */

static uint32_t * initMapMem(int fd, uint32_t addr, uint32_t len)
{
    return (uint32_t *) mmap(0, len,
       PROT_READ|PROT_WRITE|PROT_EXEC,
       MAP_SHARED|MAP_LOCKED,
       fd, addr);
}

int gpioInitialise(void)
{
   int fd;

   fd = open("/dev/mem", O_RDWR | O_SYNC) ;

   if (fd < 0)
   {
      fprintf(stderr,
         "This program needs root privileges.  Try using sudo\n");
      return -1;
   }

   gpioReg  = initMapMem(fd, GPIO_BASE, GPIO_LEN);
   clkReg   = initMapMem(fd, CLK_BASE,  CLK_LEN);

   close(fd);

   if ((gpioReg == MAP_FAILED) || (clkReg == MAP_FAILED))
   {
      fprintf(stderr,
         "Bad, mmap failed\n");
      return -1;
   }
   return 0;
}

/* ------------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
   if (gpioInitialise() < 0) return 1;

   /* int clock, int source, int divI, int divF, int MASH */
   initClock(0, 0, 4000, 0, 0);

   gpioSetMode(4, PI_ALT0);

   return 0;
}
