/*
 * ECE 3849 Real - Time Embedded Systems
 * Lab2
 * Oscilloscope
 * Spectrum Analyzer
 * with RTOS
 *
 * Gene Bogdanov    9/13/2017
 * Modifided by:
 * Jonathan Lopez   4/27/21
 * ECE Mailbox # 178
 *
 */
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

//lab 1 headers
//includes
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "driverlib/interrupt.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "Crystalfontz128x128_ST7735.h"
#include "buttons.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "sampling.h"

//spectrum
#include <math.h>
#include "kiss_fft.h"
#include "_kiss_fft_guts.h"

//defines
#define PWM_FREQUENCY 20000 // PWM frequency = 20 kHz
#define MAX_BUTTON_PRESS 10 //FIFO
#define FIFO_SIZE 10        // Maximum items in FIFO

//spectrum define
#define PI 3.14159265358979f
#define NFFT 1024 // FFT length
#define KISS_FFT_CFG_SIZE (sizeof(struct kiss_fft_state)+sizeof(kiss_fft_cpx)*(NFFT-1))

//Global Variables
//uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
extern volatile uint32_t gButtons; // from buttons.h
float scale;
float load = 0;
float unload;
float load1;
extern volatile int32_t gADCBufferIndex;                // latest sample index
extern volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE];   // circular buffer
extern volatile uint32_t gADCErrors;
volatile uint16_t sample[LCD_HORIZONTAL_MAX];
const char * const gVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", " 1 V", " 2 V"};
uint32_t gSystemClock = 120000000; // [Hz] system clock frequency
char oscMode = 1;
volatile int bufferDisp[LCD_HORIZONTAL_MAX];
float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1, 2};
int triggerSlope = 1;
int voltsperDiv = 4;

//spectrum
static char kiss_fft_cfg_buffer[KISS_FFT_CFG_SIZE]; // Kiss FFT config memory
size_t buffer_size = KISS_FFT_CFG_SIZE;
kiss_fft_cfg cfg; // Kiss FFT config
static kiss_fft_cpx in[NFFT], out[NFFT]; // complex waveform and spectrum buffers
volatile uint16_t fft_sample[NFFT];                     // need another sample buffer for the spectrum
float out_db[NFFT];                                     // buffer of db readings
//int i; //in each function

/*
 *  ======== main ========
 */
int main(void)
{
    IntMasterDisable();

    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation

    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font

    // hardware initialization goes here
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

    /* Start BIOS */
    BIOS_start();
    return (0);
}//end main
void WaveformTask(UArg arg1, UArg arg2) //highest priority
{
    IntMasterEnable();
    int i, trig;
    while (true) {
        Semaphore_pend(semaphoreWaveform, BIOS_WAIT_FOREVER);
        if(oscMode){
            trig = triggerSlope ? RisingTrigger(): FallingTrigger();//either rise or fall
            for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++){

                // Copies waveform into the local buffer
                sample[i] = gADCBuffer[ADC_BUFFER_WRAP(trig - LCD_HORIZONTAL_MAX / 2 + i)];
            }
            Semaphore_post(semaphoreProcessing);
        }else{//spectrum
            for (i = 0; i < NFFT; i++) { // generate an input waveform
                fft_sample[i] = gADCBuffer[ADC_BUFFER_WRAP(gADCBufferIndex - i)];

            }//end for
            Semaphore_post(semaphoreProcessing);
        }//end if
    }//end while 1
}//end func

void ProcessingTask(UArg arg1, UArg arg2) //lowest priority
{
    cfg = kiss_fft_alloc(NFFT, 0, kiss_fft_cfg_buffer, &buffer_size); // init Kiss FFT
    int i;
    while (true) {
        Semaphore_pend(semaphoreProcessing, BIOS_WAIT_FOREVER);
        if(oscMode){
            scale = (VIN_RANGE * PIXELS_PER_DIV) / ((1 << ADC_BITS) * fVoltsPerDiv[voltsperDiv]);
            for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++){
                bufferDisp[i] = LCD_VERTICAL_MAX / 2 - (int)roundf(scale * ((int)sample[i] - ADC_OFFSET));
            }//end for

            Semaphore_post(semaphoreDisplay);
            Semaphore_post(semaphoreWaveform);
        }else{//spectrum
            for (i = 0; i < NFFT; i++) { // generate an input waveform
                in[i].r = fft_sample[i]; // real part of waveform take out sine wave
                in[i].i = 0; // imaginary part of waveform
            }//end for 1
            kiss_fft(cfg, in, out); // compute FFT
            // convert first 128 bins of out[] to dB for display
            for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++){
                out_db[i] = 160 - ((10.0f) * log10f((out[i].r * out[i].r) + (out[i].i * out[i].i)));
            }//end for
            Semaphore_post(semaphoreDisplay);
            Semaphore_post(semaphoreWaveform);
        }
    }//end while 1
}//end func

void DisplayTask(UArg arg1, UArg arg2) //low priority
{
    //IntMasterEnable();
    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8);     // Select font
    int i, ycord, ycordP;
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};
    while (true) {
        Semaphore_pend(semaphoreDisplay, BIOS_WAIT_FOREVER);
        if(oscMode){

            GrContextForegroundSet(&sContext, ClrBlack);
            GrRectFill(&sContext, &rectFullScreen); // fill screen with black

            //blue grid
            GrContextForegroundSet(&sContext, ClrBlue);
            for(i = -3; i < 4; i++) {
                GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX - 1, LCD_VERTICAL_MAX/2 + i * PIXELS_PER_DIV);
                GrLineDrawV(&sContext, LCD_VERTICAL_MAX/2 + i * PIXELS_PER_DIV, 0, LCD_HORIZONTAL_MAX - 1);
            }
            //waveform
            GrContextForegroundSet(&sContext, ClrYellow);
            for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++){
                ycord = bufferDisp[i];
                GrLineDraw(&sContext, i, ycordP, i + 1, ycord);
                ycordP = ycord;
            }//end for
            //trigger logo, volts per div
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

            //flash
            GrFlush(&sContext); // flush the frame buffer to the LCD
        }else{//spectrum

            GrContextForegroundSet(&sContext, ClrBlack);
            GrRectFill(&sContext, &rectFullScreen); // fill screen with black

            //blue grid
            GrContextForegroundSet(&sContext, ClrBlue);
            for(i = -3; i < 4; i++) {
                GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX - 1, LCD_VERTICAL_MAX/2 + i * PIXELS_PER_DIV);
            }
            for (i = 0; i < 7; i++) {
                GrLineDrawV(&sContext, i * PIXELS_PER_DIV, 0, LCD_HORIZONTAL_MAX - 1);//fixed screen so it lines up now
            }

            //waveform
            GrContextForegroundSet(&sContext, ClrYellow);
            for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++){
                ycord = out_db[i];
                GrLineDraw(&sContext, i, ycordP, i + 1, ycord);
                ycordP = ycord;
            }//end for

            //frequency and decibel
            GrContextForegroundSet(&sContext, ClrWhite); //white text
            GrStringDraw(&sContext, "20 kHz", -1, 4, 0, false);
            GrStringDraw(&sContext, "10 dBV", -1, 50, 0, false);

            //flash
            GrFlush(&sContext); // flush the frame buffer to the LCD
        }// end else
    }//end while 1
}//end func

void ButtonTask(UArg arg1, UArg arg2)
{
    char button;
    while(1){
        Semaphore_pend(semaphoreButton, BIOS_WAIT_FOREVER);
        uint32_t gpio_buttons =
                ~GPIOPinRead(GPIO_PORTJ_BASE, 0xff) & (GPIO_PIN_1 | GPIO_PIN_0) // EK-TM4C1294XL buttons in positions 0 and 1
                | ((~GPIOPinRead(GPIO_PORTH_BASE, 0xff) & (GPIO_PIN_1)) << 1) //S1 button
                | ((~GPIOPinRead(GPIO_PORTK_BASE, 0xff) & (GPIO_PIN_6)) >> 3) //S2 button
                | ((~GPIOPinRead(GPIO_PORTD_BASE, 0xff) & (GPIO_PIN_4))); //Select button joystick
        uint32_t old_buttons = gButtons;    // save previous button state
        ButtonDebounce(gpio_buttons);       // Run the button debouncer. The result is in gButtons.
        ButtonReadJoystick();               // Convert joystick state to button presses. The result is in gButtons.
        uint32_t presses = ~old_buttons & gButtons;   // detect button presses (transitions from not pressed to pressed)
        presses |= ButtonAutoRepeat();      // autorepeat presses if a button is held long enough
        if (presses & 1) { // EK-TM4C1294XL button 1 pressed 'A'
            button = 'a';
            Mailbox_post(mailbox0, &button, BIOS_NO_WAIT);
        }
        if (presses & 2) { // EK-TM4C1294XL button 2 pressed 'B'
            button = 'b';
            Mailbox_post(mailbox0, &button, BIOS_NO_WAIT);
        }
        //joystick is button E
        //boosterpack S1 is C
        if (presses & 4) { //S1
            button = 'c';
            Mailbox_post(mailbox0, &button, BIOS_NO_WAIT);
        }
        if (presses & 8) { // button 8 pressed boosterpack S2 one trigger D
            button = 'd';
            Mailbox_post(mailbox0, &button, BIOS_NO_WAIT);
        }

    }//end while 1
}//end func

void UserInputTask(UArg arg1, UArg arg2)
{
    char button;
    while (true) {
        if(Mailbox_pend(mailbox0, &button, BIOS_WAIT_FOREVER)){
            switch(button){
            case 'a':
                voltsperDiv = voltsperDiv >= 4 ? 4 : voltsperDiv++;
                break; //extra credit lab 1
            case 'b':
                voltsperDiv = voltsperDiv <= 0 ? 0 : voltsperDiv--;
                break; //extracredit lab 1
            case 'c':
                oscMode = !oscMode; //spectrum mode now
                break;
            case'd':
                triggerSlope = !triggerSlope;
                break;
            }
            Semaphore_post(semaphoreDisplay);
        }
    }//end while

}//end func

void Clock0Task(UArg arg1, UArg arg2)
{
    Semaphore_post(semaphoreButton);

}//end func

