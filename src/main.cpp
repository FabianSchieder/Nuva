#include "stm32f1xx.h"

void delay_ms(uint32_t ms) {
    SysTick->LOAD = 72000 - 1; // 72 MHz → 1 ms
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    for (uint32_t i = 0; i < ms; i++) {
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    }

    SysTick->CTRL = 0;
}

int main(void) {
    // Clock für GPIOC aktivieren
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    // PC0 als Push-Pull Output, max 2 MHz
    GPIOC->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);

    while (1)
    {
        GPIOC->ODR ^= GPIO_ODR_ODR0;  // Toggle PC0
        delay_ms(200);               // 500 ms Pause
    }
}
