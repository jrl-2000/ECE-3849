// ECE 3849 Lab 3 source code fragments for copying and pasting


// Challenge #1

#include "driverlib/udma.h"


#pragma DATA_ALIGN(gDMAControlTable, 1024) // address alignment required
tDMAControlTable gDMAControlTable[64];     // uDMA control table (global)


SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
uDMAEnable();
uDMAControlBaseSet(gDMAControlTable);

uDMAChannelAssign(UDMA_CH24_ADC1_0); // assign DMA channel 24 to ADC1 sequence 0
uDMAChannelAttributeDisable(UDMA_SEC_CHANNEL_ADC10, UDMA_ATTR_ALL);

// primary DMA channel = first half of the ADC buffer
uDMAChannelControlSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT,
    UDMA_SIZE_16 | UDMA_SRC_INC_NONE | UDMA_DST_INC_16 | UDMA_ARB_4);
uDMAChannelTransferSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT,
    UDMA_MODE_PINGPONG, (void*)&ADC1_SSFIFO0_R,
    (void*)&gADCBuffer[0], ADC_BUFFER_SIZE/2);

// alternate DMA channel = second half of the ADC buffer
uDMAChannelControlSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT,
    UDMA_SIZE_16 | UDMA_SRC_INC_NONE | UDMA_DST_INC_16 | UDMA_ARB_4);
uDMAChannelTransferSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT,
    UDMA_MODE_PINGPONG, (void*)&ADC1_SSFIFO0_R,
    (void*)&gADCBuffer[ADC_BUFFER_SIZE/2], ADC_BUFFER_SIZE/2);

uDMAChannelEnable(UDMA_SEC_CHANNEL_ADC10);


ADCSequenceDMAEnable(ADC1_BASE, 0); // enable DMA for ADC1 sequence 0
ADCIntEnableEx(...);                // enable ADC1 sequence 0 DMA interrupt


volatile bool gDMAPrimary = true; // is DMA occurring in the primary channel?

void ADC_ISR(void)  // DMA (lab3)
{
    ADCIntClearEx(...); // clear the ADC1 sequence 0 DMA interrupt flag

    // Check the primary DMA channel for end of transfer, and restart if needed.
    if (uDMAChannelModeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT) ==
            UDMA_MODE_STOP) {
        uDMAChannelTransferSet(...); // restart the primary channel (same as setup)
        gDMAPrimary = false;    // DMA is currently occurring in the alternate buffer
    }

    // Check the alternate DMA channel for end of transfer, and restart if needed.
    // Also set the gDMAPrimary global.
    <...>

    // The DMA channel may be disabled if the CPU is paused by the debugger.
    if (!uDMAChannelIsEnabled(UDMA_SEC_CHANNEL_ADC10)) {
        uDMAChannelEnable(UDMA_SEC_CHANNEL_ADC10);  // re-enable the DMA channel
    }
}


int32_t getADCBufferIndex(void)
{
    int32_t index;
    if (gDMAPrimary) {  // DMA is currently in the primary channel
        index = ADC_BUFFER_SIZE/2 - 1 -
                uDMAChannelSizeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT);
    }
    else {              // DMA is currently in the alternate channel
        index = ADC_BUFFER_SIZE - 1 -
                uDMAChannelSizeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT);
    }
    return index;
}


// Challenge #2

#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/pin_map.h"


// configure GPIO PD0 as timer input T0CCP0 at BoosterPack Connector #1 pin 14
SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
GPIOPinTypeTimer(GPIO_PORTD_BASE, GPIO_PIN_0);
GPIOPinConfigure(...);

SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
TimerDisable(TIMER0_BASE, TIMER_BOTH);
TimerConfigure(TIMER0_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME_UP);
TimerControlEvent(TIMER0_BASE, TIMER_A, ...);
TimerLoadSet(TIMER0_BASE, TIMER_A, 0xffff);   // use maximum load value
TimerPrescaleSet(TIMER0_BASE, TIMER_A, 0xff); // use maximum prescale value
TimerIntEnable(TIMER0_BASE, ...);
TimerEnable(TIMER0_BASE, TIMER_A);


// Challenge #3

PWMGenIntTrigEnable(PWM0_BASE, PWM_GEN_2, PWM_INT_CNT_ZERO);


#include "driverlib/pwm.h"
#include "inc/tm4c1294ncpdt.h"
#include "audio_waveform.h"


uint32_t gPWMSample = 0;            // PWM sample counter
uint32_t gSamplingRateDivider = 20; // sampling rate divider

void PWM_ISR(void)
{
    PWMGenIntClear(...); // clear PWM interrupt flag

    int i = (gPWMSample++) / gSamplingRateDivider; // waveform sample index
    PWM0_2_CMPB_R = 1 + gWaveform[i]; // write directly to the PWM compare B register
    if (i ...) { // if at the end of the waveform array
        PWMIntDisable(PWM0_BASE, PWM_INT_GEN_2); // disable these interrupts
        gPWMSample = 0; // reset sample index so the waveform starts from the beginning
    }
}


