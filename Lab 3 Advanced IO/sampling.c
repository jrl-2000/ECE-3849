/*
 * sampling.c
 *
 *  Created on: Apr 6, 2021
 *  Lab 1 Digital Oscilloscope
 *      Author: Jonathan Lopez
 *      Last Modified: 5/10/21
 */
//Includes
#include <sampling.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "buttons.h"
#include "sysctl_pll.h"
#include "driverlib/adc.h"
#include "inc/tm4c1294ncpdt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/pwm.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "Crystalfontz128x128_ST7735.h"
#include "driverlib/adc.h"
#include "inc/tm4c1294ncpdt.h"
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>
#include "driverlib/udma.h"

//Defines and varibles

//volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile uint32_t gADCErrors = 0; // number of missed ADC deadlines
volatile bool gDMAPrimary = true; // is DMA occurring in the primary channel?
volatile int32_t Index2DMA;

void ADC_ISR(void){
    //lab 2
    //    ADC1_ISC_R = ADC_ISC_IN0; // clear ADC1 sequence0 interrupt flag in the ADCISC register
    //    if (ADC1_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
    //        gADCErrors++; // count errors
    //        ADC1_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
    //    }
    //    gADCBuffer[
    //               gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1)
    //               ] = ADC1_SSFIFO0_R; // read sample from the ADC1 sequence 0 FIFO

    ADCIntClearEx(ADC1_BASE, ADC_INT_DMA_SS0); // clear the ADC1 sequence 0 DMA interrupt flag
    // Check the primary DMA channel for end of transfer, and restart if needed.
    if (uDMAChannelModeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT) == UDMA_MODE_STOP) {
        uDMAChannelTransferSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT,
                               UDMA_MODE_PINGPONG, (void*)&ADC1_SSFIFO0_R,
                               (void*)&gADCBuffer[0], ADC_BUFFER_SIZE/2); // restart the primary channel (same as setup)
        gDMAPrimary = false; // DMA is currently occurring in the alternate buffer
    }//end if
    // Check the alternate DMA channel for end of transfer, and restart if needed.
    // Also set the gDMAPrimary global.
    if(uDMAChannelModeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT) == UDMA_MODE_STOP) {
        uDMAChannelTransferSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT,
                               UDMA_MODE_PINGPONG, (void*)&ADC1_SSFIFO0_R,
                               (void*)&gADCBuffer[ADC_BUFFER_SIZE/2], ADC_BUFFER_SIZE/2); // restart alternate channel
        gDMAPrimary = true;
    }//end if
    // The DMA channel may be disabled if the CPU is paused by the debugger.
    if (!uDMAChannelIsEnabled(UDMA_SEC_CHANNEL_ADC10)) {
        uDMAChannelEnable(UDMA_SEC_CHANNEL_ADC10); // re-enable the DMA channel
    }
} //end ISR

void initADC(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0); // GPIO setup for analog input AIN3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // initialize ADC peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);

    // ADC clock
    uint32_t pll_frequency = SysCtlFrequencyGet(CRYSTAL_FREQUENCY);
    uint32_t pll_divisor = (pll_frequency - 1) / (16 * ADC_SAMPLING_RATE) + 1; //round up
    ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
    ADCClockConfigSet(ADC1_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
    ADCSequenceDisable(ADC1_BASE, 0);  // choose ADC1 sequence 0; disable before configuring
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0);  // specify the "Always" trigger
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH3 | ADC_CTL_END | ADC_CTL_IE);
    // in the 0th step, sample channel 3 (AIN3)
    // enable interrupt, and make it the end of sequence

    ADCSequenceEnable(ADC1_BASE, 0);   // enable the sequence. it is now sampling
    //        ADCIntEnable(ADC1_BASE, 0);        // enable sequence 0 interrupt in the ADC1 peripheral

    ADCSequenceDMAEnable(ADC1_BASE, 0); // enable DMA for ADC1 sequence 0
    ADCIntEnableEx(ADC1_BASE, ADC_INT_DMA_SS0); // enable ADC1 sequence 0 DMA interrupt
}

int RisingTrigger(void){ // search for rising edge trigger
    Index2DMA = getADCBufferIndex();
    // Step 1
    //    int x = gADCBufferIndex - LCD_HORIZONTAL_MAX / 2; // Half screen width; don't use a magic number
    int x = Index2DMA - LCD_HORIZONTAL_MAX / 2; // Half screen width; don't use a magic number

    // Step 2
    int x_stop = x - ADC_BUFFER_SIZE/2; // Half the ADC buffer
    for (; x > x_stop; x--) {
        if (    gADCBuffer[ADC_BUFFER_WRAP(x)] >= ADC_OFFSET &&
                gADCBuffer[ADC_BUFFER_WRAP(x-1)]< ADC_OFFSET) // Older sample
            break;
    }
    // Step 3
    if (x == x_stop) // for loop ran to the end
        x = Index2DMA - LCD_HORIZONTAL_MAX / 2; // reset x back to how it was initialized
    return x;
}

int FallingTrigger(void){ // search for falling edge trigger
    Index2DMA = getADCBufferIndex();
    // Step 1
    //    int x = gADCBufferIndex - LCD_HORIZONTAL_MAX / 2 /* half screen width; don't use a magic number */;
    int x = Index2DMA - LCD_HORIZONTAL_MAX / 2 /* half screen width; don't use a magic number */;

    // Step 2
    int x_stop = x - ADC_BUFFER_SIZE/2; // Half the ADC buffer
    for (; x > x_stop; x--) {
        if (    gADCBuffer[ADC_BUFFER_WRAP(x)] < ADC_OFFSET &&
                gADCBuffer[ADC_BUFFER_WRAP(x-1)] >= ADC_OFFSET) // Older sample
            break;
    }
    // Step 3
    if (x == x_stop) // for loop ran to the end
        x = Index2DMA - LCD_HORIZONTAL_MAX / 2; // reset x back to how it was initialized
    return x;
}

int32_t getADCBufferIndex(void)
{
    int32_t index;
    IArg gatekey = GateHwi_enter(gateHwi0);
    if (gDMAPrimary) { // DMA is currently in the primary channel
        index = ADC_BUFFER_SIZE/2 - 1 -
                uDMAChannelSizeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT);
        GateHwi_leave(gateHwi0, gatekey);
    }
    else { // DMA is currently in the alternate channel
        index = ADC_BUFFER_SIZE - 1 -
                uDMAChannelSizeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT);
        GateHwi_leave(gateHwi0, gatekey);

    }
    return index;
}
