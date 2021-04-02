/*
 * main.c
 * Gene Bogdanov - 3/10/2013, modified 11/1/2017
 * ECE 3849 Real-time Embedded Systems
 * Demonstration of a shared data bug in a FIFO data structure.
 */
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/tm4c1294ncpdt.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "Crystalfontz128x128_ST7735.h"

#define delay_us(us) SysCtlDelay((us) * 40) // delay in us assuming 120 MHz operation and 3-cycle loop

uint32_t gSystemClock; // system clock frequency in Hz

// event and handler definitions
#define EVENT0_PERIOD           10007   // [us] event0 period
#define EVENT0_EXECUTION_TIME   100     // [us] event0 handler execution time

// measured event latencies in clock cycles
uint32_t event0_latency = 0;

// measured event response time in clock cycles
uint32_t event0_response_time = 0;

// number of deadlines missed
uint32_t event0_missed_deadlines = 0;

// timer periods in clock cycles (expecting 120 MHz clock)
#define TIMER0_PERIOD (120 * EVENT0_PERIOD)

// function prototypes
void event0_handler(void);

#define FIFO_SIZE 11        // FIFO capacity is 1 item fewer
typedef char DataType;      // FIFO data type
volatile DataType fifo[FIFO_SIZE];  // FIFO storage array
volatile int fifo_head = 0; // index of the first item in the FIFO
volatile int fifo_tail = 0; // index one step past the last item

// put data into the FIFO, skip if full
// returns 1 on success, 0 if FIFO was full
int fifo_put(DataType data)
{
    int new_tail = fifo_tail + 1;
    if (new_tail >= FIFO_SIZE) new_tail = 0; // wrap around
    if (fifo_head != new_tail) {    // if the FIFO is not full
        fifo[fifo_tail] = data;     // store data into the FIFO
        fifo_tail = new_tail;       // advance FIFO tail index
        return 1;                   // success
    }
    return 0;   // full
}

// get data from the FIFO
// returns 1 on success, 0 if FIFO was empty
int fifo_get(DataType *data)
{
    if (fifo_head != fifo_tail) {   // if the FIFO is not empty
        *data = fifo[fifo_head];    // read data from the FIFO
//        IntMasterDisable();
        fifo_head++;                // advance FIFO head index
//        delay_us(1000);
        if (fifo_head >= FIFO_SIZE) fifo_head = 0; // wrap around
//        IntMasterEnable();
        return 1;                   // success
    }
    return 0;   // empty
}

void main(void) {
    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation

    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};

    // initialize general purpose timer 0 for periodic interrupts
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, TIMER0_PERIOD - 1);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntPrioritySet(INT_TIMER0A, 0);
    IntEnable(INT_TIMER0A);

    IntMasterEnable();

    TimerEnable(TIMER0_BASE, TIMER_A);

    int x = 0, y = 0;
    char c;
    GrContextForegroundSet(&sContext, ClrBlack);
    GrRectFill(&sContext, &rectFullScreen); // fill screen with black
    GrContextForegroundSet(&sContext, ClrYellow);

    // main loop
    while (1) {
        delay_us(100000); // 0.1 sec
        if (fifo_get(&c)) {
            GrStringDraw(&sContext, &c, /*length*/ 1, /*x*/ x, /*y*/ y, /*opaque*/ false);
            x += 6;
            if (x > LCD_HORIZONTAL_MAX - 6) {
                x = 0;
                y += 8;
                if (y > LCD_VERTICAL_MAX - 8) {
                    y = 0;
                    GrContextForegroundSet(&sContext, ClrBlack);
                    GrRectFill(&sContext, &rectFullScreen); // fill screen with black
                    GrContextForegroundSet(&sContext, ClrYellow);
                }
            }
        }
        GrFlush(&sContext); // flush the frame buffer to the LCD
    }
}

void event0_handler(void)
{
    uint32_t t;
    t = TIMER0_PERIOD - TIMER0_TAR_R; // read Timer A count using direct register access
    if (t > event0_latency) event0_latency = t; // measure latency
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag

    int i;
    static char c = 'A';
    for (i = 0; i < 5; i++) { // attempt to put multiple items into the FIFO
        if (fifo_put(c)) {
            c++;  // go to the next char only if put was successful
            if (c > 'U') c = 'A';
        }
    }
    delay_us(EVENT0_EXECUTION_TIME); // handle event0

    if (TimerIntStatus(TIMER0_BASE, 1) & TIMER_TIMA_TIMEOUT) { // next event occurred
        event0_missed_deadlines++;
        t = 2 * TIMER0_PERIOD; // timer overflowed since last event
    }
    else t = TIMER0_PERIOD;
    t -= TimerValueGet(TIMER0_BASE, TIMER_A); // read Timer A count using driver
    if (t > event0_response_time) event0_response_time = t; // measure response time
}

//      if (fifo_head == FIFO_SIZE - 1)
//          fifo_head = 0;
//      else
//          fifo_head++;
