/**
 * main.c
 *
 * ECE 3849 Simple code execution time measurement example, comparing assembly and C code.
 * Gene Bogdanov    11/17/2017
 */

#include <stdint.h>
#include <stdbool.h>
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/systick.h"

uint32_t gSystemClock; // [Hz] system clock frequency

extern void block_copy32(uint32_t *destination, uint32_t *source, uint32_t n8);
extern void block_copy4(uint32_t *destination, uint32_t *source, uint32_t n);

void block_copy32_c(uint32_t *destination, uint32_t *source, uint32_t n8)
{
    int i;
    for (i = 0; i < n8 * 8; i++) {
        destination[i] = source[i];
    }
}

#define N_32BYTE_BLOCKS 2
uint32_t src[N_32BYTE_BLOCKS * 8];
uint32_t dst[N_32BYTE_BLOCKS * 8];

int main(void)
{
    int i;
    uint32_t bias;
    volatile uint32_t asm_cycles, asm4_cycles;
    volatile uint32_t memcpy_cycles;
    volatile uint32_t c_cycles;
    uint32_t start, finish;

    IntMasterDisable();

    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    // initialize SysTick timer for the purpose of profiling
    SysTickPeriodSet(1 << 24); // full 24-bit counter
    SysTickEnable();

    for (i = 0; i < N_32BYTE_BLOCKS * 8; i++)
        src[i] = i;

    // dry run to determine how long it takes to read the timer
    start = SysTickValueGet(); // read SysTick timer value
    finish = SysTickValueGet();
    bias = (start - finish) & 0xffffff;

    // profile the assembly function 4-byte blocks
    start = SysTickValueGet();
    block_copy4(dst, src, N_32BYTE_BLOCKS * 8);
    finish = SysTickValueGet();
    asm4_cycles = ((start - finish) & 0xffffff) - bias;

    // profile the assembly function 32-byte blocks
    start = SysTickValueGet();
    block_copy32(dst, src, N_32BYTE_BLOCKS);
    finish = SysTickValueGet();
    asm_cycles = ((start - finish) & 0xffffff) - bias;

    // profile the naive C block copy function - for loop
    start = SysTickValueGet();
    block_copy32_c(dst, src, N_32BYTE_BLOCKS);
    finish = SysTickValueGet();
    c_cycles = ((start - finish) & 0xffffff) - bias;

    // profile the memcpy function
    start = SysTickValueGet();
    memcpy(dst, src, N_32BYTE_BLOCKS * 32);
    finish = SysTickValueGet();
    memcpy_cycles = ((start - finish) & 0xffffff) - bias;

    while (1) {

    }
}
