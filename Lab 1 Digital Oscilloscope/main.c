/**
 * main.c
 *
 * ECE 3849 Lab 1 Digital Oscilloscope
 * Jonathan Lopez    3/24/2021
 *
 *
 */
#include <main.h>
#include <sampling.h>
#include <stdint.h>
#include <stdbool.h>
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>
#include "buttons.h"
#include <math.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"

//Make a .h file!
//Variables
uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
extern volatile uint32_t gButtons; // from buttons.h
float scale;
float load = 0;
float unload;
float load1;
extern volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
extern volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
extern volatile uint32_t gADCErrors = 0; // number of missed ADC deadlines
const char * const gVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", " 1 V", " 2 V"};

int main(void)
{
    IntMasterDisable();

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

    uint32_t time;  // local copy of gTime
    char str[50];   // string buffer
    unsigned int mins;
    unsigned int secs;
    unsigned int fracs;

    uint32_t buttonP;
    //mm:ss:ff
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};
    ButtonInit();
    IntMasterEnable();
    //
    // configure M0PWM2, at GPIO PF2, BoosterPack 1 header C1 pin 2
    // configure M0PWM3, at GPIO PF3, BoosterPack 1 header C1 pin 3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPinConfigure(GPIO_PF3_M0PWM3);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3,
     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    // configure the PWM0 peripheral, gen 1, outputs 2 and 3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_1); // use system clock without division
    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, roundf((float)gSystemClock/PWM_FREQUENCY));
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, roundf((float)gSystemClock/PWM_FREQUENCY*0.4f));
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_3, roundf((float)gSystemClock/PWM_FREQUENCY*0.4f));
    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT | PWM_OUT_3_BIT, true);
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);

    while (true) {
        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black
        time = gTime; // read shared global only once
        buttonP = gButtons;

        //TODO Modify the snprintf() call in the infinite while loop to reformat the time into the desired mm:ss:ff (should display “Time = 01:23:45” instead of “Time = 008345” that this code produces). If you need documentation on the C library function snprintf(), look it up on the web.
        //note to self: remember time is in 1/100 of second
        mins = time / 6000;
        secs = (time/100) % 60;
        fracs = time % 100;

        snprintf(str, sizeof(str), "Time = %02d:%02d:%02d", mins, secs, fracs); // convert time to string
        GrContextForegroundSet(&sContext, ClrYellow); // yellow text
        GrStringDraw(&sContext, str, /*length*/ -1, /*x*/ 0, /*y*/ 0, /*opaque*/ false);

        //TODO display button states
        snprintf(str, sizeof(str), "%u%u%u%u%u%u%u%u%u", (buttonP>>8)&1, (buttonP>>7)&1, (buttonP>>6)&1, (buttonP>>5)&1, (buttonP>>4)&1, (buttonP>>3)&1, (buttonP>>2)&1, (buttonP>>1)&1, (buttonP>>0)&1); //%u is for a unsigned integer and bit shift them over
        GrStringDraw(&sContext, str, /*length*/ -1, /*x*/ 0, /*y*/ 16, /*opaque*/ false); // make y 16 so it doesn't write over the stopwatch display line

        GrFlush(&sContext); // flush the frame buffer to the LCD
    }
}
