/*
 *
 * Authors: Fabian Schieder, Christopher Müllner
 * Copyright: S&M Solutions
 * Version: 1.0.0
 *
 */

#ifndef CORTEX_STM32F103_H
#define CORTEX_STM32F103_H

#include <string>


#define D0  *((volatile unsigned long *)(BITBAND_PERI(GPIOC_IDR,0)))		// PC0
#define D1  *((volatile unsigned long *)(BITBAND_PERI(GPIOC_IDR,1)))	 	// PC1
#define D2  *((volatile unsigned long *)(BITBAND_PERI(GPIOC_IDR,2)))		// PC2
#define D3  *((volatile unsigned long *)(BITBAND_PERI(GPIOC_IDR,3)))	 	// PC3
#define D4  *((volatile unsigned long *)(BITBAND_PERI(GPIOB_IDR,7))) 	    // PB7
#define D5  *((volatile unsigned long *)(BITBAND_PERI(GPIOB_IDR,6)))	 	// PB6
#define D6  *((volatile unsigned long *)(BITBAND_PERI(GPIOC_IDR,12))) 	    // PC12
#define D7  *((volatile unsigned long *)(BITBAND_PERI(GPIOC_IDR,13)))	    // PC13


#define AIN0  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,0)))	// PA0
#define AIN1 *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,1)))	// PA1
#define AIN2  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,2)))	// PA2
#define AIN3  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,3)))	// PA3
#define AIN4  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,4)))	// PA4
#define D11  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,5)))	// PA5
#define AIN6  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,6)))	// PA6
#define AIN5  *((volatile unsigned long *)(BITBAND_PERI(GPIOA_ODR,7)))	// PA7


typedef struct SensorData
{
    float temperature;
    float humidity;
    std::string time;
} SensorData;






class Cortex_STM32F103
{
    private:
        SensorData sensorData;
        
};


#endif //CORTEX_STM32F103_H