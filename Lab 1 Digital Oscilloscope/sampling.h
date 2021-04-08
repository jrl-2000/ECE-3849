/*
 * sampling.h
 *
 *  Created on: Apr 8, 2021
 *      Author: Jonathan Lopez
 */

#ifndef SAMPLING_H_
#define SAMPLING_H_
#include <stdint.h> //forgot this


//Function prototypes
//Note to self put these in a .h file
void initADC(void);
int RisingTrigger(void);
int FallingTrigger(void);
void ADC_ISR(void);

#define VIN_RANGE 3.3                                    // total V_in range
#define PIXELS_PER_DIV 20                                // LCD pixels per voltage division
#define ADC_BITS 12                                      // number of bits in ADC
#define ADC_OFFSET 2048
#define ADC_BUFFER_SIZE 2048 // size must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile uint32_t gADCErrors = 0; // number of missed ADC deadlines

#endif /* SAMPLING_H_ */
