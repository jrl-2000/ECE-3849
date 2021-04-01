/*
 * main.c
 * Gene Bogdanov - 3/8/2013, modified by Jennifer Stander 3/18/2021
 * ECE 3849 Real-time Embedded Systems
 * Code for measuring real-time task latency, response time and
 * determining if deadlines are met.  Additional code for measuring
 * CPU utilization by ISRs is also included.
 * Try event2 handler execution time of 2500, 3500 and 6000.
 * 2500 is schedulable using round-robin polling.
 * 3500 is schedulable using prioritized polling.
 * 6000 is schedulable using prioritized interrupts.
 * However, disabling interrupts for even a short time in the main loop
 * breaks the schedule.
 */

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
// #include "inc/hw_ints.h"
#include "inc/tm4c1294ncpdt.h"
#include "driverlib/fpu.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"

// delay in us assuming 120 MHz operation and 3-cycle loop
// sysctldelay takes num. of 3 clock loops: 40 loops in 1 usec
#define delay_us(us) SysCtlDelay((us) * 40)

uint32_t gSystemClock; // [Hz] system clock frequency

#define ROUND_ROBIN_POLLING  1
//#define PRIORITY_POLLING     1

// event and handler definitions
#define EVENT0_PERIOD           6007    // [us] event0 period
#define EVENT0_EXECUTION_TIME   2000    // [us] event0 handler execution time

#define EVENT1_PERIOD           8101    // [us] event1 period
#define EVENT1_EXECUTION_TIME   1000    // [us] event1 handler execution time

#define EVENT2_PERIOD           12301   // [us] event2 period
#define EVENT2_EXECUTION_TIME   2500    // [us] event2 handler execution time

// build options
//#define ENABLE_ISR 1
//#define DISABLE_INTERRUPTS_IN_ISR // if defined, interrupts are disabled in the body of each ISR
//#define DISABLE_INTERRUPTS_SHORT 1
//#define ENABLE_CPU_LOAD_COUNT 1

// measured event latencies in clock cycles
uint32_t event0_latency = 0;
uint32_t event1_latency = 0;
uint32_t event2_latency = 0;

// measured event response time in clock cycles
uint32_t event0_response_time = 0;
uint32_t event1_response_time = 0;
uint32_t event2_response_time = 0;

// number of deadlines missed
uint32_t event0_missed_deadlines = 0;
uint32_t event1_missed_deadlines = 0;
uint32_t event2_missed_deadlines = 0;

// timer periods in clock cycles (expecting 120 MHz clock)
// 120 clock cycles in 1 usec
#define TIMER0_PERIOD (120 * EVENT0_PERIOD)
#define TIMER1_PERIOD (120 * EVENT1_PERIOD)
#define TIMER2_PERIOD (120 * EVENT2_PERIOD)

// CPU load counters
uint32_t count_unloaded = 0;
uint32_t count_loaded = 0;
float cpu_load = 0.0;

// function prototypes
void event0_handler(void);
void event1_handler(void);
void event2_handler(void);
#pragma FUNC_CANNOT_INLINE(cpu_load_count)
uint32_t cpu_load_count(void);

void main(void) {
    IntMasterDisable();

    // Enable the Floating Point Unit
    FPUEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    // initialize general purpose timers 0-2 for periodic interrupts
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, TIMER0_PERIOD - 1);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntPrioritySet(INT_TIMER0A, 0); // 0 = highest priority, 32 = next lower
    IntEnable(INT_TIMER0A);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    TimerDisable(TIMER1_BASE, TIMER_BOTH);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_A, TIMER1_PERIOD - 1);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    IntPrioritySet(INT_TIMER1A, 32); // 0 = highest priority, 32 = next lower
    IntEnable(INT_TIMER1A);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    TimerDisable(TIMER2_BASE, TIMER_BOTH);
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER2_BASE, TIMER_A, TIMER2_PERIOD - 1);
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    IntPrioritySet(INT_TIMER2A, 64); // 0 = highest priority, 32 = next lower
    IntEnable(INT_TIMER2A);

#if defined(ENABLE_ISR) || defined(ENABLE_CPU_LOAD_COUNT)
    IntMasterEnable(); // comment for polled scheduling or CPU load testing
#endif

#ifndef ENABLE_CPU_LOAD_COUNT
    TimerEnable(TIMER0_BASE, TIMER_A); // comment for CPU load testing
    TimerEnable(TIMER1_BASE, TIMER_A);
    TimerEnable(TIMER2_BASE, TIMER_A);
#endif
    // schedule the tasks without interrupts (using polling instead)

#ifdef ROUND_ROBIN_POLLING
    while (true) {
        if (TimerIntStatus(TIMER0_BASE, 1) & TIMER_TIMA_TIMEOUT) {  // event 0 has occurred
            event0_handler();
        }
        if (    TimerIntStatus(TIMER1_BASE, 1) & TIMER_TIMA_TIMEOUT) {  // event 1 has occurred
            event1_handler();
        }
        if (TimerIntStatus(TIMER2_BASE, 1) & TIMER_TIMA_TIMEOUT) {  // event 2 has occurred
                event2_handler();
            }
        }

#endif

#ifdef PRIORITY_POLLING
    while (true) {
        if (TimerIntStatus(TIMER0_BASE, 1) & TIMER_TIMA_TIMEOUT) {  // event 0 has occurred
            event0_handler();
        }
        else if (TimerIntStatus(TIMER1_BASE, 1) & TIMER_TIMA_TIMEOUT) {  // event 1 has occurred
            event1_handler();
        }
            else if (TimerIntStatus(TIMER2_BASE, 1) & TIMER_TIMA_TIMEOUT) {  // event 2 has occurred
            event2_handler();
        }
    }
#endif

#ifndef DISABLE_INTERRUPTS_SHORT
#ifdef ENABLE_ISR
    while(true) {

    }
#endif
#endif
#ifdef DISABLE_INTERRUPTS_SHORT
    //loop for testing the effect of disabling interrupts
    while (true) {
        IntMasterDisable();
        delay_us(100);
//        count_unloaded++;
//        count_loaded--;
        IntMasterEnable();

        IntMasterDisable();
        delay_us(200);
        IntMasterEnable();
    }
#endif
    //////////////////////////////////////////////////////////////////////////////
    // code for keeping track of CPU load
#ifdef ENABLE_CPU_LOAD_COUNT

    // initialize timer 3 in one-shot mode for polled timing
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    TimerDisable(TIMER3_BASE, TIMER_BOTH);
    TimerConfigure(TIMER3_BASE, TIMER_CFG_ONE_SHOT);
    TimerLoadSet(TIMER3_BASE, TIMER_A, gSystemClock - 1); // 1 sec interval

    count_unloaded = cpu_load_count();

    IntMasterEnable();

    TimerEnable(TIMER0_BASE, TIMER_A);
    TimerEnable(TIMER1_BASE, TIMER_A);
    TimerEnable(TIMER2_BASE, TIMER_A);

    while (1) {
        count_loaded = cpu_load_count();
        cpu_load = 1.0f - (float)count_loaded/count_unloaded; // compute CPU load
    }


#endif
}

void event0_handler(void)
{
#ifdef DISABLE_INTERRUPTS_IN_ISR
    IntMasterDisable();
#endif
    uint32_t t = TIMER0_PERIOD - TIMER0_TAR_R; // read Timer A count using direct register access
    if (t > event0_latency) event0_latency = t; // measure latency
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag

    delay_us(EVENT0_EXECUTION_TIME); // handle event0

    if (TimerIntStatus(TIMER0_BASE, 1) & TIMER_TIMA_TIMEOUT) { // next event occurred
        event0_missed_deadlines++;
        t = 2 * TIMER0_PERIOD; // timer overflowed since last event
    }
    else t = TIMER0_PERIOD;
    t -= TimerValueGet(TIMER0_BASE, TIMER_A); // read Timer A count using driver
    if (t > event0_response_time) event0_response_time = t; // measure response time
#ifdef DISABLE_INTERRUPTS_IN_ISR
    IntMasterEnable();
#endif
}

void event1_handler(void)
{
#ifdef DISABLE_INTERRUPTS_IN_ISR
    IntMasterDisable();
#endif
    uint32_t t = TIMER1_PERIOD - TIMER1_TAR_R;
    if (t > event1_latency) event1_latency = t;
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag

    delay_us(EVENT1_EXECUTION_TIME); // handle event1

    if (TimerIntStatus(TIMER1_BASE, 1) & TIMER_TIMA_TIMEOUT) { // next event occurred
        event1_missed_deadlines++;
        t = 2 * TIMER1_PERIOD; // timer overflowed since last event
    }
    else t = TIMER1_PERIOD;
    t -= TimerValueGet(TIMER1_BASE, TIMER_A);
    if (t > event1_response_time) event1_response_time = t;
#ifdef DISABLE_INTERRUPTS_IN_ISR
    IntMasterEnable();
#endif
}

void event2_handler(void)
{
#ifdef DISABLE_INTERRUPTS_IN_ISR
    IntMasterDisable();
#endif
    uint32_t t = TIMER2_PERIOD - TIMER2_TAR_R;
    if (t > event2_latency) event2_latency = t;
    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag

//  IntMasterDisable();
////    IntDisable(INT_TIMER0A);
//  delay_us(100);
////    IntEnable(INT_TIMER0A);
//  IntMasterEnable();

    delay_us(EVENT2_EXECUTION_TIME); // handle event2

    if (TimerIntStatus(TIMER2_BASE, 1) & TIMER_TIMA_TIMEOUT) { // next event occurred
        event2_missed_deadlines++;
        t = 2 * TIMER2_PERIOD; // timer overflowed since last event
    }
    else t = TIMER2_PERIOD;
    t -= TimerValueGet(TIMER2_BASE, TIMER_A);
    if (t > event2_response_time) event2_response_time = t;
#ifdef DISABLE_INTERRUPTS_IN_ISR
    IntMasterEnable();
#endif
}

uint32_t cpu_load_count(void)
{
    uint32_t i = 0;
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer
    while (!(TimerIntStatus(TIMER3_BASE, false) & TIMER_TIMA_TIMEOUT))
        i++;
    return i;
}
