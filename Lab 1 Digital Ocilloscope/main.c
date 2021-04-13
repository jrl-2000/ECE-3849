/**
 * main.c
 *
 * ECE 3849 Lab 1 Digital Oscilloscope
 * Jonathan Lopez    3/24/2021
 * Completed 4/12/21
 *
 */

//includes
#include <sampling.h>
#include "buttons.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "Crystalfontz128x128_ST7735.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"

//Global Variables
uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
extern volatile uint32_t gButtons; // from buttons.h
float scale;
float load = 0;
float unload;
float load1;
extern volatile int32_t gADCBufferIndex;                // latest sample index
extern volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE];   // circular buffer
extern volatile uint32_t gADCErrors;
const char * const gVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", " 1 V", " 2 V"};

//defines
#define PWM_FREQUENCY 20000 // PWM frequency = 20 kHz
#define MAX_BUTTON_PRESS 10 //FIFO
#define FIFO_SIZE 10        // Maximum items in FIFO

//Function prototypes
uint32_t CPULoad(void);

//main loop
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

    ButtonInit();
    initADC();

    //Source Voltage Init
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
    //other variables
    int ycordP, i, ycord, trig;
    char button;
    int voltsperDiv = 4;
    int triggerSlope = 1;
    char str1[50];
    float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1, 2};
    uint16_t sample[LCD_HORIZONTAL_MAX];  // Sample on LCD

    unload = CPULoad(); //unload
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};
    IntMasterEnable();
    //main while loop
    while (true){
        load = CPULoad();
        load1 = 1.0 - (float)load/unload;
        while (fifo_get(&button)){
            switch(button){
            case 'a':
                voltsperDiv = ++voltsperDiv > 4 ? 4 : voltsperDiv++;
                break;
            case 'b':
                voltsperDiv = --voltsperDiv > 4 ? 4 : voltsperDiv--;
                break;
            case'e':
                triggerSlope = !triggerSlope;
                break;
            }
        }

        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black
        //blue grid
        GrContextForegroundSet(&sContext, ClrBlue);
        for(i = -3; i < 4; i++) {
            GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX - 1, LCD_VERTICAL_MAX/2 + i * PIXELS_PER_DIV);
            GrLineDrawV(&sContext, LCD_VERTICAL_MAX/2 + i * PIXELS_PER_DIV, 0, LCD_HORIZONTAL_MAX - 1);
        }
        waveform
        GrContextForegroundSet(&sContext, ClrYellow);
        trig = triggerSlope ? RisingTrigger(): FallingTrigger();
        scale = (VIN_RANGE * PIXELS_PER_DIV) / ((1 << ADC_BITS) * fVoltsPerDiv[voltsperDiv]);
        for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++)
        {
            // Copy waveform into local buffer
            sample[i] = gADCBuffer[ADC_BUFFER_WRAP(trig - LCD_HORIZONTAL_MAX / 2 + i)];

            // draw lines
            ycord = LCD_VERTICAL_MAX / 2 - (int)roundf(scale * ((int)sample[i] - ADC_OFFSET));
            GrLineDraw(&sContext, i, ycordP, i + 1, ycord);
            ycordP = ycord;
        }
        //trigger, volts per div, cpu load
        GrContextForegroundSet(&sContext, ClrWhite); //white text
        if(triggerSlope){
            GrLineDraw(&sContext, 105, 10, 115, 10);
            GrLineDraw(&sContext, 115, 10, 115, 0);
            GrLineDraw(&sContext, 115, 0, 125, 0);
            GrLineDraw(&sContext, 112, 6, 115, 2);
            GrLineDraw(&sContext, 115, 2, 118, 6);
        }else{
            GrLineDraw(&sContext, 105, 10, 115, 10);
            GrLineDraw(&sContext, 115, 10, 115, 0);
            GrLineDraw(&sContext, 115, 0, 125, 0);
            GrLineDraw(&sContext, 112, 3, 115, 7);
            GrLineDraw(&sContext, 115, 7, 118, 3);
        }
        GrStringDraw(&sContext, "20 us", -1, 4, 0, false);
        GrStringDraw(&sContext, gVoltageScaleStr[voltsperDiv], -1, 50, 0, false);
        snprintf(str1, sizeof(str1), "CPU load = %.1f%%", load1*100);
        GrStringDraw(&sContext, str1, -1, 0, 120, false);

        GrFlush(&sContext); // flush the frame buffer to the LCD
    } //end of while
}// end of main

//cpu load function
uint32_t CPULoad(void){
    uint32_t i = 0;
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER3_BASE,TIMER_A);
    while (!(TimerIntStatus(TIMER3_BASE, false) & TIMER_TIMA_TIMEOUT)) {
        i++;
    }
    return i;
}

