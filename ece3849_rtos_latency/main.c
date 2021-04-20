/*
 * main.c
 * Gene Bogdanov - 3/18/2013
 * ECE 3849 Real-time Embedded Systems
 * Code for measuring RTOS task latency, response time and
 * determining if deadlines are met.  Runs under SYS/BIOS.
 */
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/tm4c1294ncpdt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>
#include <ti/sysbios/BIOS.h>

#define SYSTEM_CLOCK_MHZ 120            // [MHz] system clock frequency
#define delay_us(us) SysCtlDelay((us) * SYSTEM_CLOCK_MHZ / 3) // delay in us assuming a 3-cycle loop

// event and handler definitions
#define EVENT0_PERIOD           6007    // [us] event0 period
#define EVENT0_EXECUTION_TIME   2000    // [us] event0 handler execution time

#define EVENT1_PERIOD           8101    // [us] event1 period
#define EVENT1_EXECUTION_TIME   1000    // [us] event1 handler execution time

#define EVENT2_PERIOD           12301   // [us] event2 period
#define EVENT2_EXECUTION_TIME   6000    // [us] event2 handler execution time

//#define ENABLE_DEADLOCK 1
//#define PRIORITY_INVERSION 1
//#define ENABLE_GATEMUTEXPRI 1
//#define ENABLE_GATETASK 1
// build options
//#define DISABLE_INTERRUPTS_IN_ISR // if defined, interrupts are disabled in the body of each ISR

// measured event latencies in clock cycles
unsigned long event0_latency = 0;
unsigned long event1_latency = 0;
unsigned long event2_latency = 0;

// measured event response time in clock cycles
unsigned long event0_response_time = 0;
unsigned long event1_response_time = 0;
unsigned long event2_response_time = 0;

// number of deadlines missed
unsigned long event0_missed_deadlines = 0;
unsigned long event1_missed_deadlines = 0;
unsigned long event2_missed_deadlines = 0;

// timer periods in clock cycles
#define TIMER0_PERIOD (SYSTEM_CLOCK_MHZ * EVENT0_PERIOD)
#define TIMER1_PERIOD (SYSTEM_CLOCK_MHZ * EVENT1_PERIOD)
#define TIMER2_PERIOD (SYSTEM_CLOCK_MHZ * EVENT2_PERIOD)

unsigned long event0_count = 0;
unsigned long event1_count = 0;
unsigned long event2_count = 0;

void main(void) {
    IntMasterDisable();

    // initialize general purpose timers 0-2 for periodic interrupts
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, TIMER0_PERIOD - 1);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntClear(TIMER0_BASE,  TIMER_TIMA_TIMEOUT);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    TimerDisable(TIMER1_BASE, TIMER_BOTH);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_A, TIMER1_PERIOD - 1);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntClear(TIMER1_BASE,  TIMER_TIMA_TIMEOUT);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    TimerDisable(TIMER2_BASE, TIMER_BOTH);
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER2_BASE, TIMER_A, TIMER2_PERIOD - 1);
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntClear(TIMER2_BASE,  TIMER_TIMA_TIMEOUT);

    BIOS_start();
}

void ISR_Event0(UArg arg0)
{
    TIMER0_ICR_R = TIMER_ICR_TATOCINT; // clear interrupt flag
    Semaphore_post(semaphore0);
}

void ISR_Event1(UArg arg0)
{
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
    Semaphore_post(semaphore1);
}

void ISR_Event2(UArg arg0)
{
    TIMER2_ICR_R = TIMER_ICR_TATOCINT;
    Semaphore_post(semaphore2);
}

void task0_func(UArg arg0, UArg arg1)
{
    unsigned long t;
    IArg key;

    while(1) {
        Semaphore_pend(semaphore0, BIOS_WAIT_FOREVER);

        t = TIMER0_PERIOD - TIMER0_TAR_R;
        if (t > event0_latency) event0_latency = t; // measure latency

#ifdef ENABLE_DEADLOCK
      Semaphore_pend(sem_a, BIOS_WAIT_FOREVER);
      Semaphore_pend(sem_b, BIOS_WAIT_FOREVER);
      delay_us(100); // "critical section"
      Semaphore_post(sem_b);
      Semaphore_post(sem_a);

#endif

#ifdef PRIORITY_INVERSION
      Semaphore_pend(sem_a, BIOS_WAIT_FOREVER);
      delay_us(100); // "critical section"
      Semaphore_post(sem_a);
#endif

#ifdef ENABLE_GATEMUTEXPRI
        key = GateMutexPri_enter(gateMutexPri0);
        delay_us(100); // "critical section"
        GateMutexPri_leave(gateMutexPri0, key);
#endif

#ifdef ENABLE_GATETASK
        key = GateTask_enter(gateTask0);
        delay_us(100); // "critical section"
        GateTask_leave(gateTask0, key);
#endif
        delay_us(EVENT0_EXECUTION_TIME); // handle event0
        event0_count++;

        if (Semaphore_getCount(semaphore0)) { // next event occurred
            event0_missed_deadlines++;
            t = 2 * TIMER0_PERIOD; // timer overflowed since last event
        }
        else t = TIMER0_PERIOD;
        t -= TIMER0_TAR_R;
        if (t > event0_response_time) event0_response_time = t; // measure response time
    }
}

void task1_func(UArg arg0, UArg arg1)
{
    unsigned long t;

    while(1) {
        Semaphore_pend(semaphore1, BIOS_WAIT_FOREVER);

        t = TIMER1_PERIOD - TIMER1_TAR_R;
        if (t > event1_latency) event1_latency = t;

//      Semaphore_pend(mutex0, BIOS_WAIT_FOREVER);
//      delay_us(100); // "critical section"
//      Semaphore_post(mutex0);

        delay_us(EVENT1_EXECUTION_TIME); // handle event1
        event1_count++;

        if (Semaphore_getCount(semaphore1)) { // next event occurred
            event1_missed_deadlines++;
            t = 2 * TIMER1_PERIOD; // timer overflowed since last event
        }
        else t = TIMER1_PERIOD;
        t -= TIMER1_TAR_R;
        if (t > event1_response_time) event1_response_time = t;
    }
}

void task2_func(UArg arg0, UArg arg1)
{
    unsigned long t;
    IArg key;

    IntMasterEnable();

    TimerEnable(TIMER0_BASE, TIMER_A);
    TimerEnable(TIMER1_BASE, TIMER_A);
    TimerEnable(TIMER2_BASE, TIMER_A);

    while(1) {
        Semaphore_pend(semaphore2, BIOS_WAIT_FOREVER);

        t = TIMER2_PERIOD - TIMER2_TAR_R;
        if (t > event2_latency) event2_latency = t;

#ifdef ENABLE_DEADLOCK
      Semaphore_pend(sem_b, BIOS_WAIT_FOREVER);
      delay_us(100);
      Semaphore_pend(sem_a, BIOS_WAIT_FOREVER);
      delay_us(100); // "critical section"
      Semaphore_post(sem_a);
      Semaphore_post(sem_b);
#endif

#ifdef PRIORITY_INVERSION
      Semaphore_pend(sem_a, BIOS_WAIT_FOREVER);
      delay_us(100); // "critical section"
      Semaphore_post(sem_a);
#endif

#ifdef ENABLE_GATEMUTEXPRI
        key = GateMutexPri_enter(gateMutexPri0);
        delay_us(100); // "critical section"
        GateMutexPri_leave(gateMutexPri0, key);
#endif

#ifdef ENABLE_GATETASK
        key = GateTask_enter(gateTask0);
        delay_us(100); // "critical section"
        GateTask_leave(gateTask0, key);
#endif

//      key = GateHwi_enter(gateHwi0);
//      delay_us(100); // "critical section"
//      GateHwi_leave(gateHwi0, key);

        delay_us(EVENT2_EXECUTION_TIME); // handle event2
        event2_count++;

        if (Semaphore_getCount(semaphore2)) { // next event occurred
            event2_missed_deadlines++;
            t = 2 * TIMER2_PERIOD; // timer overflowed since last event
        }
        else t = TIMER2_PERIOD;
        t -= TIMER2_TAR_R;
        if (t > event2_response_time) event2_response_time = t;
    }
}
