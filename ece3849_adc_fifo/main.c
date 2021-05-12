/*
 * main.c
 * Gene Bogdanov - 4/11/2013, modified 11/21/2017
 * ECE 3849 Real-time Embedded Systems
 * Demonstrates the effect of the ADC FIFO on CPU utilization and tolerance for latency.
 */
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/tm4c1294ncpdt.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"


#define ADC_FIFO_NONE 1
//#define ADC_FIFO_ENABLE 1
//#define ADC_FIFO_SIZE2 1
//#define ADC_FIFO_SIZE4 1
//#define ADC_FIFO_SIZE8 1

//#define DISABLE_INTERRUPTS 1
//#define INTERRUPT_DELAY 1


#define delay_us(us) SysCtlDelay((us) * 40) // delay in us assuming 120 MHz operation and 3-cycle loop

#define ADC_SAMPLING_RATE 1000000 // [samples/sec] desired ADC sampling rate
#define PLL_FREQUENCY 480000000 // [Hz] PLL frequency (this system uses a divider of 2 after the 480 MHz PLL)

uint32_t gSystemClock; // system clock frequency in Hz
uint32_t gADCSamplingRate;  // [Hz] actual ADC samplaing rate

// CPU load counters
uint32_t count_unloaded = 0;
uint32_t count_loaded = 0;
float cpu_load = 0.0;

uint32_t cpu_load_count(void)
{
    uint32_t i = 0;
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer
    while (!(TimerIntStatus(TIMER3_BASE, 0) & TIMER_TIMA_TIMEOUT))
        i++;
    return i;
}

void main(void) {
    IntMasterDisable();

    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    // initialize ADC peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
    // ADC clock
    uint32_t pll_divisor = (PLL_FREQUENCY - 1) / (16 * ADC_SAMPLING_RATE) + 1; // round divisor up
    gADCSamplingRate = PLL_FREQUENCY / (16 * pll_divisor); // actual sampling rate may differ from ADC_SAMPLING_RATE
    ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor); // only ADC0 has PLL clock divisor control
    ADCClockConfigSet(ADC1_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
    // use analog input AIN3, at GPIO PE0, which is BoosterPack 1 header B1 (2nd from left) pin 3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);

    ADCSequenceDisable(ADC1_BASE, 0);
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0);

#ifdef ADC_FIFO_NONE
    // single-sample interrupts
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH0);
#endif

#ifdef ADC_FIFO_SIZE8
    // configure sequence as a FIFO, interrupting every 8 samples
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 1, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 2, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 3, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 4, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 5, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 6, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 7, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
#endif


#ifdef ADC_FIFO_SIZE4
    // configure sequence as a FIFO, interrupting every 4 samples
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 1, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 2, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 3, ADC_CTL_CH0 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 4, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 5, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 6, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 7, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
#endif

#ifdef ADC_FIFO_SIZE2
    // configure sequence as a FIFO, interrupting every 2 samples
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 1, ADC_CTL_CH0 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 2, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 3, ADC_CTL_CH0 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 4, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 5, ADC_CTL_CH0 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 6, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 7, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
#endif


    ADCSequenceEnable(ADC1_BASE, 0);
    ADCIntEnable(ADC1_BASE, 0);
    IntPrioritySet(INT_ADC1SS0, 0);
    IntEnable(INT_ADC1SS0);

    //////////////////////////////////////////////////////////////////////////////
    // code for keeping track of CPU load

    // initialize timer 3 in one-shot mode for polled timing
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    TimerDisable(TIMER3_BASE, TIMER_BOTH);
    TimerConfigure(TIMER3_BASE, TIMER_CFG_ONE_SHOT);
    TimerLoadSet(TIMER3_BASE, TIMER_A, gSystemClock/100 - 1); // 10 ms interval

    count_unloaded = cpu_load_count();
    count_loaded = cpu_load_count();

    IntMasterEnable();

    while (1) {
        count_loaded = cpu_load_count();
        cpu_load = 1.0f - (float)count_loaded/count_unloaded; // compute CPU load
#ifdef DISABLE_INTERRUPTS
        IntMasterDisable();  // test the effect of increasing interrupt latency
        delay_us(INTERRUPT_DELAY);
        IntMasterEnable();
#endif
    }
}

#define ADC_BUFFER_SIZE 2048
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1))
int gADCBufferIndex = ADC_BUFFER_SIZE - 1;
uint16_t gADCBuffer[ADC_BUFFER_SIZE];
uint32_t gADCErrors = 0;

#ifdef ADC_FIFO_NONE
// single-sample version
void ADC_ISR(void)
{
    ADC1_ISC_R = ADC_ISC_IN0; // clear interrupt flag (must be done early)
    if (ADC1_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
        gADCErrors++;   // count errors
        ADC1_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
    }
    // grab and save the A/D conversion result
    gADCBuffer[
               gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1)
               ] = ADC1_SSFIFO0_R;
}
#endif

#ifdef ADC_FIFO_ENABLE
// FIFO version
void ADC_ISR(void)
{
    ADC1_ISC_R = ADC_ISC_IN0; // clear interrupt flag (must be done early)
    if (ADC1_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
        gADCErrors++;   // count errors
        ADC1_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
    }
    // empty out the ADC FIFO
    while (!(ADC1_SSFSTAT0_R & ADC_SSFSTAT0_EMPTY)) {
        gADCBuffer[
                   gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1)
                   ] = ADC1_SSFIFO0_R;
    }
}
#endif
