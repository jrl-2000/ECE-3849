/*
 * main.h
 *
 *  Created on: Apr 8, 2021
 *      Author: Jonathan Lopez
 */

#ifndef MAIN_H_
#define MAIN_H_

#define PWM_FREQUENCY 20000 // PWM frequency = 20 kHz
#define MAX_BUTTON 10 //FIFO
#define FIFO_SIZE 10        // Maximum items in FIFO

uint32_t CPULoad(void);

#endif /* MAIN_H_ */
